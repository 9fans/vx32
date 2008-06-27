
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <setjmp.h>
#include <assert.h>
#include <errno.h>

#include <vxa/vxa.h>
#include <vxa/codec.h>

#include "cdjpeg.h"
#include "native.h"


char vxacodec_name[] = "jpeg";


// Defined in djpeg-bin.c, generated from djpeg by bin2c.pl
extern const uint8_t vxa_djpeg_data[];
extern const int vxa_djpeg_length;


int vxacodec_init(vxacodec *c, vxaio *io)
{
	c->decoder = vxa_djpeg_data;
	c->decodersize = vxa_djpeg_length;

	return VXA_RC_OK;
}

int vxacodec_encode(struct vxacodec *c, struct vxaio *io)
{
	return vxa_error(io, VXA_RC_WRONG_FORMAT,
			"jpeg codec currently doesn't support compression");
}

static void error_exit(j_common_ptr cinfo)
{
	struct client_data *cdata = cinfo->client_data;

	// Jump back to the original entrypoint with an error return
	longjmp(cdata->errjmp, 1);
}

static void output_message(j_common_ptr cinfo)
{
	struct client_data *cdata = cinfo->client_data;

	/* Create the message */
	char buffer[JMSG_LENGTH_MAX];
	cinfo->err->format_message(cinfo, buffer);

	/* Stuff it into the vxaio struct */
	vxa_error(cdata->io, VXA_RC_CORRUPT_DATA, "%s", buffer);
}

static void init_source(j_decompress_ptr cinfo)
{
	// Nothing to do
}

static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
	struct client_data *cdata = cinfo->client_data;

	ssize_t inlen = cdata->io->readf(cdata->io, cdata->inbuf, BUFSIZE);
	if (inlen == 0)
		vxa_error(cdata->io, VXA_RC_CORRUPT_DATA,
			"compressed JPEG stream is truncated");
	if (inlen <= 0)
		longjmp(cdata->errjmp, 1);

	cinfo->src->next_input_byte = cdata->inbuf;
	cinfo->src->bytes_in_buffer = inlen;

	return TRUE;
}

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	struct client_data *cdata = cinfo->client_data;

	while (num_bytes > cinfo->src->bytes_in_buffer) {
		num_bytes -= cinfo->src->bytes_in_buffer;
		fill_input_buffer(cinfo);
	}
	cinfo->src->next_input_byte += num_bytes;
	cinfo->src->bytes_in_buffer -= num_bytes;
}

static void term_source(j_decompress_ptr cinfo)
{
	// Nothing to do
}

void vxajpeg_flush(struct client_data *cd)
{
	if (cd->outpos == 0)
		return;

	ssize_t rc = cd->io->writef(cd->io, cd->outbuf, cd->outpos);
	if (rc < 0)
		longjmp(cd->errjmp, 1);
	cd->outpos = 0;
}

int vxacodec_decode(struct vxacodec *c, struct vxaio *io)
{
	// Save the vital args in a client_data struct for our callbacks
	struct client_data cdata;
	cdata.io = io;
	cdata.inbuf = NULL;

	// Set up the JPEG library error handler struct
	struct jpeg_error_mgr jerr;
	memset(&jerr, 0, sizeof(jerr));
	jpeg_std_error(&jerr);
	jerr.error_exit = error_exit;
	jerr.output_message = output_message;

	// Setup the JPEG input source
	struct jpeg_source_mgr jsrc;
	memset(&jsrc, 0, sizeof(jsrc));
	jsrc.init_source = init_source;
	jsrc.fill_input_buffer = fill_input_buffer;
	jsrc.skip_input_data = skip_input_data;
	jsrc.resync_to_restart = jpeg_resync_to_restart;
	jsrc.term_source = term_source;

	// Set up the main decompression state struct
	struct jpeg_decompress_struct cinfo;
	memset(&cinfo, 0, sizeof(cinfo));
	cinfo.client_data = &cdata;
	cinfo.err = &jerr;
	cinfo.src = &jsrc;

	// Set up a JMP_BUF for error_exit() to longjmp to on fatal errors. */
	if (setjmp(cdata.errjmp)) {
		jpeg_destroy_decompress(&cinfo);
		if (cdata.inbuf != NULL)
			free(cdata.inbuf);
		return io->errcode;
	}

	// Allocate the input and output buffers
	cdata.inbuf = malloc(BUFSIZE*2);
	if (cdata.inbuf == NULL)
		return vxa_error(io, VXA_RC_NO_MEMORY,
				"no memory for JPEG decoder input buffer");
	cdata.outbuf = cdata.inbuf + BUFSIZE;
	cdata.outpos = 0;

	// Initialize the JPEG decoder
	jpeg_create_decompress(&cinfo);
	(void) jpeg_read_header(&cinfo, TRUE);

	djpeg_dest_ptr dest_mgr = jinit_write_bmp(&cinfo, FALSE);
	dest_mgr->output_file = (FILE*)&cdata;	// XX hack hack hack

	(void) jpeg_start_decompress(&cinfo);
	dest_mgr->start_output(&cinfo, dest_mgr);

	/* Process data */
	while (cinfo.output_scanline < cinfo.output_height) {
		JDIMENSION num_scanlines = jpeg_read_scanlines(
				&cinfo, dest_mgr->buffer,
				dest_mgr->buffer_height);
		dest_mgr->put_pixel_rows(&cinfo, dest_mgr, num_scanlines);
	}

	dest_mgr->finish_output(&cinfo, dest_mgr);

	vxajpeg_flush(&cdata);

	(void) jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	free(cdata.inbuf);

	return VXA_RC_OK;
}

int vxacodec_recognize(struct vxacodec *c, struct vxaio *io,
			const void *header, size_t size)
{
	const uint8_t *inp = header;
	if (size >= 4 &&
			inp[0] == 0xff &&
			inp[1] == 0xd8 &&	// SOI
			inp[2] == 0xff &&
			inp[3] >= 0xe0) {	// APPx
		io->method = VXA_M_JPEG;
		return vxa_error(io, VXA_RC_COMPRESSED,
				"input already compressed in JPEG format");
	}

	return vxa_error(io, VXA_RC_WRONG_FORMAT,
			"JPEG codec currently can't compress");
}


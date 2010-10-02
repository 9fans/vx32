
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <vxa/vxa.h>
#include <vxa/codec.h>

#include "zlib.h"


#define BUFSIZE (64*1024)

char vxacodec_name[] = "zlib";


// Defined in dzlib-bin.c, generated from dzlib by bin2c.pl
extern const uint8_t vxa_dzlib_data[];
extern const int vxa_dzlib_length;


// Translate a zlib return code into a VXA return code
static int xlrc(vxaio *io, z_stream *zs, int zrc)
{
	switch (zrc) {
	case Z_OK:
		return VXA_RC_OK;
	case Z_STREAM_END:
		return vxa_error(io, VXA_RC_EOF, "end of file");
	case Z_MEM_ERROR:
		return vxa_error(io, VXA_RC_NO_MEMORY, "out of memory in zlib");
	case Z_STREAM_ERROR:
		return vxa_error(io, VXA_RC_INVALID_ARG,
				"zlib detected invalid argument");
	default:
		if (zs->msg)
			return vxa_error(io, VXA_RC_UNKNOWN,
					"zlib error: %s", zs->msg);
		else
			return vxa_error(io, VXA_RC_UNKNOWN,
					"unknown zlib error: %d", zrc);
	}
}

int vxacodec_init(struct vxacodec *c, struct vxaio *io)
{
	c->decoder = vxa_dzlib_data;
	c->decodersize = vxa_dzlib_length;
}

int vxacodec_encode(vxacodec *c, vxaio *io)
{
	io->method = VXA_M_DEFLATE;

	z_stream zs;
	memset(&zs, 0, sizeof(zs));

	// Allocate input and output data buffers
	char *iobuf = malloc(BUFSIZE*2);
	if (iobuf == NULL) {
		return vxa_error(io, VXA_RC_NO_MEMORY,
			"No memory for zlib I/O buffers");
	}
	char *inbuf = iobuf;
	char *outbuf = iobuf + BUFSIZE;

	// Just encode input data into a raw deflate stream,
	// leaving the client to do any wrapping (e.g., ZIP)
	int zrc = deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
			-15, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	if (zrc != Z_OK) {
	    enderr:
		free(iobuf);
		return xlrc(io, &zs, zrc);
	}

	// Compress the input stream
	ssize_t inlen;
	while ((inlen = io->readf(io, inbuf, BUFSIZE)) > 0) {

		// Compress this input block
		zs.next_in = inbuf;
		zs.avail_in = inlen;
		do {
			zs.next_out = outbuf;
			zs.avail_out = BUFSIZE;
			if ((zrc = deflate(&zs, 0)) != Z_OK) {
				zerr:
				deflateEnd(&zs);
				free(iobuf);
				return xlrc(io, &zs, zrc);
			}

			// Write any output the decompressor produced
			ssize_t outlen = io->writef(io, outbuf,
						BUFSIZE - zs.avail_out);
			if (outlen < 0)
				goto ioerr;

			// Continue decompressing the input block until done
		} while (zs.avail_in > 0);
	}
	if (inlen < 0) {
		ioerr:
		deflateEnd(&zs);
		free(iobuf);
		return io->errcode;
	}

	// Flush the output
	zs.avail_in = 0;
	int done = 0;
	do {
		zs.next_out = outbuf;
		zs.avail_out = BUFSIZE;
		zrc = deflate(&zs, Z_FINISH);
		if (zrc == Z_STREAM_END)
			done = 1;
		else if (zrc != Z_OK)
			goto zerr;

		// Write compressor output
		ssize_t outlen = io->writef(io, outbuf,
					BUFSIZE - zs.avail_out);
		if (outlen < 0)
			goto ioerr;

	} while (!done);

	zrc = deflateEnd(&zs);
	if (zrc != Z_OK)
		goto enderr;
	free(iobuf);
	return 0;
}

int vxacodec_decode(vxacodec *c, vxaio *io)
{
	// Decode gzip format or raw deflate stream as appropriate
	int wbits;
	switch (io->method) {
	case VXA_M_GZIP:
		wbits = 16+15;	// Decode GZIP format stream
		break;
	case VXA_M_DEFLATE:
		wbits = -15;	// Decode raw deflate stream
		break;
	default:
		return vxa_error(io, VXA_RC_WRONG_FORMAT,
				"zlib: unsupported method code %08x",
				io->method);
	}

	z_stream zs;
	memset(&zs, 0, sizeof(zs));

	// Allocate input and output data buffers
	char *iobuf = malloc(BUFSIZE*2);
	if (iobuf == NULL) {
		return vxa_error(io, VXA_RC_NO_MEMORY,
			"No memory for zlib I/O buffers");
	}
	char *inbuf = iobuf;
	char *outbuf = iobuf + BUFSIZE;

	// Initialize the decompressor in the appropriate mode.
	int zrc = inflateInit2(&zs, wbits);
	if (zrc != Z_OK) {
		enderr:
		free(iobuf);
		return xlrc(io, &zs, zrc);
	}

	// Deompress the input stream
	int done = 0;
	do {
		// Read a block of input data
		ssize_t inlen = io->readf(io, inbuf, BUFSIZE);
		if (inlen == 0)
			vxa_error(io, VXA_RC_CORRUPT_DATA,
				"compressed input data truncated");
		if (inlen <= 0) {
			ioerr:
			inflateEnd(&zs);
			free(iobuf);
			return io->errcode;
		}

		// Decompress this input block
		zs.next_in = inbuf;
		zs.avail_in = inlen;
		do {
			zs.next_out = outbuf;
			zs.avail_out = BUFSIZE;
			zrc = inflate(&zs, 0);
			if (zrc == Z_STREAM_END)
				done = 1;
			else if (zrc != Z_OK) {
				zerr:
				inflateEnd(&zs);
				free(iobuf);
				return xlrc(io, &zs, zrc);
			}

			// Write any output the decompressor produced
			ssize_t outlen = io->writef(io, outbuf,
						BUFSIZE - zs.avail_out);
			if (outlen < 0)
				goto ioerr;

			// Continue decompressing until done
		} while (zs.avail_in > 0 && !done);
	} while (!done);

	zrc = inflateEnd(&zs);
	if (zrc != Z_OK)
		goto enderr;

	free(iobuf);

	return VXA_RC_OK;
}

int vxacodec_recognize(vxacodec *c, vxaio *io, const void *header, size_t size)
{
	const uint8_t *inp = header;

	// See if the initial data provided looks like a GZIP file.
	if (size >= 3 && inp[0] == 0x1f && inp[1] == 0x8b && inp[2] == 8) {
		// Apparently a GZIP stream...
		io->method = VXA_M_GZIP;	// "gzip"
		return vxa_error(io, VXA_RC_COMPRESSED,
				"input already compressed in gzip format");
	}

	// Format not recognized, but we could try compressing it anyway...
	return VXA_RC_OK;
}


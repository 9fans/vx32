
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include <vxa/vxa.h>
#include <vxa/codec.h>

#include "bzlib.h"


#define BUFSIZE (64*1024)

char vxacodec_name[] = "bzip2";


// Defined in dzlib-bin.c, generated from dzlib by bin2c.pl
extern const uint8_t vxa_dbz2_data[];
extern const int vxa_dbz2_length;


void bz_internal_error (int errcode)
{
	fprintf(stderr, "Internal bzip2 error: %d", errcode);
	abort();
}

// Translate a bzlib return code into a VXA return code
static int xlrc(vxaio *io, int bzrc)
{
	switch (bzrc) {
	case BZ_OK:
	case BZ_RUN_OK:
	case BZ_FLUSH_OK:
	case BZ_FINISH_OK:
		return VXA_RC_OK;
	case BZ_STREAM_END:
		return vxa_error(io, VXA_RC_EOF, "end of file");
	case BZ_MEM_ERROR:
		return vxa_error(io, VXA_RC_NO_MEMORY,
				"out of memory in bzlib");
	default:
		return vxa_error(io, VXA_RC_UNKNOWN,
				"unknown bzip2 error (%d)", bzrc);
	}
}

int vxacodec_init(struct vxacodec *c, struct vxaio *io)
{
	c->decoder = vxa_dbz2_data;
	c->decodersize = vxa_dbz2_length;
}

int vxacodec_encode(struct vxacodec *c, struct vxaio *io)
{
	io->method = VXA_M_BZIP2;

	bz_stream bzs;
	memset(&bzs, 0, sizeof(bzs));

	// Allocate input and output data buffers
	char *iobuf = malloc(BUFSIZE*2);
	if (iobuf == NULL) {
		return vxa_error(io, VXA_RC_NO_MEMORY,
			"No memory for bzip2 I/O buffers");
	}
	char *inbuf = iobuf;
	char *outbuf = iobuf + BUFSIZE;

	// Initialize the bzip2 compressor
	int bzrc = BZ2_bzCompressInit(&bzs, 9, 0, 0);
	if (bzrc != BZ_OK) {
	    enderr:
		free(iobuf);
		return xlrc(io, bzrc);
	}

	// Read and compress the input stream
	ssize_t inlen;
	while ((inlen = io->readf(io, inbuf, BUFSIZE)) > 0) {

		// Compress this input block
		bzs.next_in = inbuf;
		bzs.avail_in = inlen;
		do {
			bzs.next_out = outbuf;
			bzs.avail_out = BUFSIZE;
			bzrc = BZ2_bzCompress(&bzs, BZ_RUN);
			if (bzrc != BZ_RUN_OK) {
			    bzerr:
				BZ2_bzCompressEnd(&bzs);
				free(iobuf);
				return xlrc(io, bzrc);
			}

			// Write any output the decompressor produced
			ssize_t outlen = io->writef(io, outbuf,
						BUFSIZE - bzs.avail_out);
			if (outlen < 0)
				goto ioerr;

			// Continue decompressing the input block until done
		} while (bzs.avail_in > 0);
	}
	if (inlen < 0) {
		ioerr:
		BZ2_bzCompressEnd(&bzs);
		free(iobuf);
		return io->errcode;
	}

	// Flush the output
	bzs.avail_in = 0;
	int done = 0;
	do {
		bzs.next_out = outbuf;
		bzs.avail_out = BUFSIZE;
		bzrc = BZ2_bzCompress(&bzs, BZ_FINISH);
		if (bzrc == BZ_STREAM_END)
			done = 1;
		else if (bzrc != BZ_FINISH_OK)
			goto bzerr;

		// Write compressor output
		ssize_t outlen = io->writef(io, outbuf,
					BUFSIZE - bzs.avail_out);
		if (outlen < 0)
			goto ioerr;

	} while (!done);

	bzrc = BZ2_bzCompressEnd(&bzs);
	if (bzrc != BZ_OK)
		goto enderr;

	return 0;
}

int vxacodec_decode(struct vxacodec *c, struct vxaio *io)
{
	if (io->method != VXA_M_BZIP2)
		return vxa_error(io, VXA_RC_WRONG_FORMAT,
				"bzip2: unsupported method code %08x",
				io->method);

	bz_stream bzs;
	memset(&bzs, 0, sizeof(bzs));

	// Allocate input and output data buffers
	char *iobuf = malloc(BUFSIZE*2);
	if (iobuf == NULL) {
		return vxa_error(io, VXA_RC_NO_MEMORY,
			"No memory for bzip2 I/O buffers");
	}
	char *inbuf = iobuf;
	char *outbuf = iobuf + BUFSIZE;

	// Initialize the bzip2 decompressor
	int bzrc;
	if ((bzrc = BZ2_bzDecompressInit(&bzs, 0, 0)) != BZ_OK) {
		enderr:
		free(iobuf);
		return xlrc(io, bzrc);
	}

	// Decompress the input stream
	int done = 0;
	do {
		// Read a block of input data
		ssize_t inlen = io->readf(io, inbuf, BUFSIZE);
		if (inlen == 0)
			vxa_error(io, VXA_RC_CORRUPT_DATA,
				"bzip2-compressed input data truncated");
		if (inlen < 0) {
			ioerr:
			BZ2_bzDecompressEnd(&bzs);
			free(iobuf);
			return io->errcode;
		}

		// Decompress this input block
		bzs.next_in = inbuf;
		bzs.avail_in = inlen;
		do {
			bzs.next_out = outbuf;
			bzs.avail_out = BUFSIZE;
			bzrc = BZ2_bzDecompress(&bzs);
			if (bzrc == BZ_STREAM_END)
				done = 1;
			else if (bzrc != BZ_OK) {
				bzerr:
				BZ2_bzDecompressEnd(&bzs);
				free(iobuf);
				return xlrc(io, bzrc);
			}

			// Write any output the decompressor produced
			ssize_t outlen = io->writef(io, outbuf,
						BUFSIZE - bzs.avail_out);
			if (outlen < 0)
				goto ioerr;

			// Continue decompressing the input block until done
		} while (bzs.avail_in > 0 && !done);
	} while (!done);

	if ((bzrc = BZ2_bzDecompressEnd(&bzs)) != BZ_OK)
		goto enderr;

	free(iobuf);

	return VXA_RC_OK;
}

int vxacodec_recognize(struct vxacodec *c, struct vxaio *io,
			const void *header, size_t size)
{
	const uint8_t *inp = header;

	// See if the initial data provided looks like a GZIP file.
	if (size >= 10 &&
			inp[0] == 'B' &&	// BZIP2 header
			inp[1] == 'Z' &&
			inp[2] == 'h' &&
			inp[3] >= '1' && inp[3] <= '9' &&
			inp[4] == 0x31 &&	// Block header
			inp[5] == 0x41 &&
			inp[6] == 0x59 &&
			inp[7] == 0x26 &&
			inp[8] == 0x53 &&
			inp[9] == 0x59) {
		io->method = VXA_M_BZIP2;
		return vxa_error(io, VXA_RC_COMPRESSED,
				"input already compressed in bzip2 format");
	}

	// Format not recognized, but we could try compressing it anyway...
	return VXA_RC_OK;
}


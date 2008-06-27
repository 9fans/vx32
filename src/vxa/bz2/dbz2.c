
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include "bzlib.h"


#define BUFSIZE (64*1024)

static bz_stream s;
static char inbuf[BUFSIZE];
static char outbuf[BUFSIZE];


void fatal(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(2);
}

void bz_internal_error (int errcode)
{
	fatal("Internal bzip2 error: %d", errcode);
}

int main(int argc, char **argv)
{
	while (1) {
		int rc;
		if ((rc = BZ2_bzDecompressInit(&s, 0, 0)) != BZ_OK)
			fatal("BZ2_bzDecompressInit: %d", rc);

		// Decompress the input file
		int done = 0;
		do {
			// Read a block of input data
			ssize_t inlen = read(STDIN_FILENO, inbuf, BUFSIZE);
			if (inlen < 0)
				fatal("read error: %d", errno);
			if (inlen == 0)
				fatal("compressed data appears to be truncated");

			// Decompress this input block
			s.next_in = inbuf;
			s.avail_in = inlen;
			do {
				s.next_out = outbuf;
				s.avail_out = BUFSIZE;
				rc = BZ2_bzDecompress(&s);
				if (rc == BZ_STREAM_END)
					done = 1;
				else if (rc != BZ_OK)
					fatal("BZ2_bzDecompress: %d", rc);

				// Write any output the decompressor produced
				ssize_t outlen = write(STDOUT_FILENO, outbuf,
							BUFSIZE - s.avail_out);
				if (outlen < 0)
					fatal("write error: %d", errno);

				// Continue decompressing the input block until done
			} while (s.avail_in > 0 && !done);
		} while (!done);

		if ((rc = BZ2_bzDecompressEnd(&s)) != BZ_OK)
			fatal("BZ2_bzDecompressEnd: %d", rc);

		// Indicate to parent that we're done
		//asm volatile("syscall" : : "a" (VXAPC_DONE) : "ecx");
		return 0;
	}
}


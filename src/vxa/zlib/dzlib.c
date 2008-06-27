
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

//#include <vxa/vxa.h>

#include "zlib.h"


#define BUFSIZE (64*1024)

static z_stream s;
static uint8_t inbuf[BUFSIZE];
static uint8_t outbuf[BUFSIZE];


void fatal(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(2);
}

int main(int argc, char **argv)
{
	int rc;
	if ((rc = inflateInit2(&s, -15)) != Z_OK)
		fatal("inflateInit: %d", rc);

	while (1) {
		// Decompress the input file
		int done = 0;
		do {
			// Read a block of input data
			ssize_t inlen = read(STDIN_FILENO, inbuf, BUFSIZE);
			if (inlen < 0)
				fatal("read error: %d", errno);
			if (inlen == 0)
				fatal("compressed data truncated");

			// Decompress this input block
			s.next_in = inbuf;
			s.avail_in = inlen;
			do {
				s.next_out = outbuf;
				s.avail_out = BUFSIZE;
				rc = inflate(&s, 0);
				if (rc == Z_STREAM_END)
					done = 1;
				else if (rc != Z_OK)
					fatal("inflate: %d", rc);

				// Write any output the decompressor produced
				ssize_t outlen = write(STDOUT_FILENO, outbuf,
							BUFSIZE - s.avail_out);
				if (outlen < 0)
					fatal("write error: %d", errno);

				// Continue decompressing until done
			} while (s.avail_in > 0 && !done);
		} while (!done);

		// Indicate to parent that we're done
		//asm volatile("syscall" : : "a" (VXAPC_DONE) : "ecx");
		return(0);

		// Get zlib ready for the next stream
		if ((rc = inflateReset(&s)) != Z_OK)
			fatal("inflateReset: %d", rc);
	}
}



#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include "zlib.h"


#define BUFSIZE (64*1024)

static z_stream s;
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

int main(int argc, char **argv)
{
	int rc = deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
				-15, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	if (rc != Z_OK)
		fatal("deflateInit: %d", rc);

	// Compress the input file
	ssize_t inlen;
	while ((inlen = read(STDIN_FILENO, inbuf, BUFSIZE)) > 0) {

		// Compress this input block
		s.next_in = inbuf;
		s.avail_in = inlen;
		do {
			s.next_out = outbuf;
			s.avail_out = BUFSIZE;
			if ((rc = deflate(&s, 0)) != Z_OK)
				fatal("deflate: %d", rc);

			// Write any output the decompressor produced
			ssize_t outlen = write(STDOUT_FILENO, outbuf,
						BUFSIZE - s.avail_out);
			if (outlen < 0)
				fatal("write error: %d", errno);

			// Continue decompressing the input block until done
		} while (s.avail_in > 0);
	}
	if (inlen < 0)
		fatal("read error: %d", errno);

	// Flush the output
	s.avail_in = 0;
	int done = 0;
	do {
		s.next_out = outbuf;
		s.avail_out = BUFSIZE;
		int rc = deflate(&s, Z_FINISH);
		if (rc == Z_STREAM_END)
			done = 1;
		else if (rc != Z_OK)
			fatal("deflate: %d", rc);

		// Write compressor output
		ssize_t outlen = write(STDOUT_FILENO, outbuf,
					BUFSIZE - s.avail_out);
		if (outlen < 0)
			fatal("write error: %d", errno);

	} while (!done);

	rc = deflateEnd(&s);
	if (rc != Z_OK)
		fatal("deflateEnd: %d", rc);

	return 0;
}


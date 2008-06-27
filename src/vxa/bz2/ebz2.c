
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
printf("malloc3: %08x\n", malloc(123));
printf("malloc3: %08x\n", malloc(123));
printf("malloc3: %08x\n", malloc(123));
printf("malloc3: %08x\n", malloc(123));
printf("malloc4: %08x\n", malloc(1234));
printf("malloc4: %08x\n", malloc(1234));
printf("malloc4: %08x\n", malloc(1234));
printf("malloc4: %08x\n", malloc(1234));
printf("malloc5: %08x\n", malloc(12345));
printf("malloc5: %08x\n", malloc(12345));
printf("malloc5: %08x\n", malloc(12345));
printf("malloc5: %08x\n", malloc(12345));
printf("malloc6: %08x\n", malloc(123456));
printf("malloc6: %08x\n", malloc(123456));
printf("malloc6: %08x\n", malloc(123456));
printf("malloc6: %08x\n", malloc(123456));
printf("malloc7: %08x\n", malloc(1234567));
printf("malloc7: %08x\n", malloc(1234567));
printf("malloc7: %08x\n", malloc(1234567));
printf("malloc7: %08x\n", malloc(1234567));
	int rc = BZ2_bzCompressInit(&s, 9, 0, 0);
	if (rc != BZ_OK)
		fatal("BZ2_bzCompressInit: %d", rc);

	// Compress the input file
	ssize_t inlen;
	while ((inlen = read(STDIN_FILENO, inbuf, BUFSIZE)) > 0) {

		// Compress this input block
		s.next_in = inbuf;
		s.avail_in = inlen;
		do {
			s.next_out = outbuf;
			s.avail_out = BUFSIZE;
			if ((rc = BZ2_bzCompress(&s, BZ_RUN)) != BZ_RUN_OK)
				fatal("BZ2_bzCompress: %d", rc);

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
		int rc = BZ2_bzCompress(&s, BZ_FINISH);
		if (rc == BZ_STREAM_END)
			done = 1;
		else if (rc != BZ_FINISH_OK)
			fatal("BZ2_bzCompress: %d", rc);

		// Write compressor output
		ssize_t outlen = write(STDOUT_FILENO, outbuf,
					BUFSIZE - s.avail_out);
		if (outlen < 0)
			fatal("write error: %d", errno);

	} while (!done);

	rc = BZ2_bzCompressEnd(&s);
	if (rc != BZ_OK)
		fatal("BZ2_bzCompressEnd: %d", rc);

	return 0;
}


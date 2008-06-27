
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>

#include "vorbis/vorbisfile.h"


#define BUFSIZE (64*1024)

static char buf[BUFSIZE];


void fatal(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(2);
}


FILE *fopen(const char *path, const char *mode) { assert(0); }
int fseek(FILE *stream, long offset, int whence) { return -1; }
long ftell(FILE *stream) { assert(0); }
int fclose(FILE *fp) { assert(0); }
void perror(const char *s) { assert(0); }


int main(int argc, char **argv)
{
	OggVorbis_File vf;
	int rc = ov_open(stdin, &vf, NULL, 0);
	if (rc != 0)
		fatal("ov_open: %d", rc);

	while (1) {
		long act = ov_read(&vf, buf, BUFSIZE, 0, 2, 1, NULL);
		if (act < 0)
			fatal("ov_read: %d", rc);
		if (act == 0)
			break;

		rc = write(STDOUT_FILENO, buf, act);
		if (rc < 0)
			fatal("write: %d", rc);
	}

	return 0;
}


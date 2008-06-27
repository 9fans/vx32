
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>

#include "jasper/jasper.h"


void fatal(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(2);
}


int open(const char *path, int flags, ...) { assert(0); }
off_t lseek(int fildes, off_t offset, int whence) { assert(0); }
int close(int fd) { return 0; }
double atof(const char *nptr) { assert(0); }
int unlink(const char *path) { assert(0); }
char *tmpnam(char *s) { assert(0); }
int fseek(FILE *stream, long offset, int whence) { assert(0); }
int fclose(FILE *fp) { assert(0); }


int main(int argc, char **argv)
{
	int fmtid = 0;
	jas_image_fmtops_t fmtops;

	fmtops.decode = jp2_decode;
	fmtops.encode = NULL;
	fmtops.validate = jp2_validate;
	jas_image_addfmt(fmtid, "jp2", "jp2",
          "JPEG-2000 JP2 File Format Syntax (ISO/IEC 15444-1)", &fmtops);
	++fmtid;

	fmtops.decode = NULL;
	fmtops.encode = bmp_encode;
	fmtops.validate = NULL;
	jas_image_addfmt(fmtid, "bmp", "bmp",
		"Microsoft Bitmap (BMP)", &fmtops);
	++fmtid;

	jas_stream_t *in = jas_stream_fdopen(STDIN_FILENO, "rb");
	assert(in != NULL);

	jas_stream_t *out = jas_stream_fdopen(STDOUT_FILENO, "w+b");
	assert(out != NULL);

	jas_image_t *image = jas_image_decode(in, 0, NULL);
	assert(image != NULL);

	int rc = jas_image_encode(image, out, 1, NULL);
	assert(rc == 0);

	jas_stream_flush(out);

	jas_stream_close(in);
	jas_stream_close(out);

	jas_image_destroy(image);

	return 0;
}


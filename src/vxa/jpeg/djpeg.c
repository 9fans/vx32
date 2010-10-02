
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>

#include "cdjpeg.h"


void fatal(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(2);
}


static struct jpeg_decompress_struct cinfo;
static struct jpeg_error_mgr jerr;

int main(int argc, char **argv)
{
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, stdin);
	(void) jpeg_read_header(&cinfo, TRUE);

	djpeg_dest_ptr dest_mgr = jinit_write_bmp(&cinfo, FALSE);
	dest_mgr->output_file = stdout;

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

	(void) jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	return 0;
}


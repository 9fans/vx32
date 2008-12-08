
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>

#include "FLAC/file_decoder.h"
#include "src/flac/decode.h"

void fatal(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(2);
}

int fseek(FILE *stream, long offset, int whence) { assert(0); }
long ftell(FILE *stream) { assert(0); }
FILE *fopen(const char *path, const char *mode) { assert(0); }
int fclose(FILE *fp) { assert(0); }
int unlink(const char *path) { assert(0); }
int stat(const char *file_name, struct stat *buf) { assert(0); }
double atof(const char *nptr) { assert(0); }

const char *grabbag__file_get_basename(const char *path) { return path; }
FILE *grabbag__file_get_binary_stdout() { return stdout; }

void grabbag__replaygain_load_from_vorbiscomment() { assert(0); }
void grabbag__replaygain_compute_scale_factor() { assert(0); }
void FLAC__replaygain_synthesis__init_dither_context(
	DitherContext *dither, int bits, int shapingtype) { assert(0); }
size_t FLAC__replaygain_synthesis__apply_gain(FLAC__byte *data_out, FLAC__bool little_endian_data_out, FLAC__bool unsigned_data_out, const FLAC__int32 * const input[], unsigned wide_samples, unsigned channels, const unsigned source_bps, const unsigned target_bps, const double scale, const FLAC__bool hard_limit, FLAC__bool do_dithering, DitherContext *dither_context) { assert(0); }

void flac__analyze_init(analysis_options aopts) { assert(0); }
void flac__analyze_frame(const FLAC__Frame *frame, unsigned frame_number, analysis_options aopts, FILE *fout) { assert(0); }
void flac__analyze_finish(analysis_options aopts) { assert(0); }



int main(int argc, char **argv)
{
	static wav_decode_options_t wavopts;
	static analysis_options aopts;

	close(2);  // sorry

	flac__utils_parse_skip_until_specification(NULL,
			&wavopts.common.skip_specification);
	flac__utils_parse_skip_until_specification(NULL,
			&wavopts.common.until_specification);
	wavopts.common.until_specification.is_relative = true;

	int rc = flac__decode_wav("-", "-", 0, aopts, wavopts);
	if (rc != 0)
		fatal("flac__decode_wav: %d", rc);

	return 0;
}


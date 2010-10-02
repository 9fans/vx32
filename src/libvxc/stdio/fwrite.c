
#include <string.h>

#include "ioprivate.h"


size_t fwrite(const void *restrict buf, size_t eltsize, size_t nelts,
		FILE *restrict f)
{
	size_t totsize = eltsize * nelts;

	// Check for space in the output buffer.
	if (f->opos + totsize > f->omax) {

		// Create/flush the output buffer.
		if (__getospace(f) < 0)
			return EOF;

		if (totsize >= f->omax) {

			// Bigger than the buffer - just write it directly.
			if (__writebuf(f, buf, totsize) < 0)
				return 0;
			return nelts;
		}
	}

	// Copy the data to the output buffer.
	memcpy(&f->obuf[f->opos], buf, totsize);
	f->opos += totsize;

	// Flush the buffer if appropriate.
	if ((f->bufmode == _IOLBF && memchr(buf, '\n', totsize)) ||
			(f->bufmode == _IONBF)) {
		if (fflush(f) < 0)
			return 0;
	}

	return nelts;
}


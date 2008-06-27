
#include "ioprivate.h"

int fgetc(FILE *f)
{
	// Make sure at least one character is available.
	if (f->ipos >= f->ilim) {
		if (__getinput(f) < 0)
			return EOF;
	}

	// Grab and return one.
	return f->ibuf[f->ipos++];
}


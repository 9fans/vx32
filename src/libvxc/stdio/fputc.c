
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "ioprivate.h"

int fputc(int c, FILE *f)
{
	c &= 0xff;

	// Make sure we have output buffer space for one character.
	if (f->opos >= f->omax) {
		if (__getospace(f) < 0)
			return EOF;
	}

	// Add the character to the buffer
	f->obuf[f->opos++] = c;

	// Flush the buffer if appropriate.
	if ((f->bufmode == _IOLBF && c == '\n') || (f->bufmode == _IONBF)) {
		if (fflush(f) < 0)
			return EOF;
	}

	return c;
}


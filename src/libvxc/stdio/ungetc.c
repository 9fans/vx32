#include "ioprivate.h"

int ungetc(int c, FILE *f)
{
	if (c == EOF)
		return -1;
	if (f->ipos == 0)
		return -1;
	if (f->isstring) {
		--f->ipos;
		return c & 0xFF;
	}
	f->ibuf[--f->ipos] = c;
	return c & 0xFF;
}

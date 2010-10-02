
#include "ioprivate.h"

int fflush(FILE *f)
{
	int rc = __writebuf(f, f->obuf, f->opos);
	f->opos = 0;
	return rc;
}


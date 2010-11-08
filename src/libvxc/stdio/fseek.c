#include <stdio.h>
#include <unistd.h>

int fseek(FILE *f, long offset, int whence)
{
	fflush(f);
	long off = lseek(f->fd, offset, whence);
	if (off < 0)
		return -1;
	f->ipos = 0;
	f->ilim = 0;
	f->ioffset = off;
	f->opos = 0;
	return 0;
}

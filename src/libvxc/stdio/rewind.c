#include <stdio.h>

void rewind(FILE *f)
{
	fseek(f, 0, SEEK_SET);
	f->errflag = 0;
}


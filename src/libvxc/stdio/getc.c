
#include <stdio.h>

#undef getc
int getc(FILE *f)
{
	return fgetc(f);
}



#include <stdio.h>

#undef putc
int putc(int c, FILE *f)
{
	fputc(c, f);
}


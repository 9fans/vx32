#include <stdio.h>

long ftell(FILE *fin)
{
	return (long)ftello(fin);
}

off_t ftello(FILE *fin)
{
	return fin->ioffset + fin->ipos;
}


#include <stdio.h>

#include "ioprivate.h"


int fputs(const char *s, FILE *f)
{
	int c;
	while ((c = *s++) != 0) {
		if (putc(c, f) < 0)
			return EOF;
	}
	return 0;
}

int puts(const char *s)
{
	if (fputs(s, stdout) < 0)
		return EOF;
	if (putchar('\n') < 0)
		return EOF;
	return 0;
}


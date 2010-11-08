
#include <stdio.h>

#undef putchar
int putchar(int c)
{
	putc(c, stdout);
}


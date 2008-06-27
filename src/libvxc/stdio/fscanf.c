#include <stdio.h>
#include <stdarg.h>

int fscanf(FILE *f, const char *fmt, ...)
{
	va_list arg;
	
	va_start(arg, fmt);
	int n = vfscanf(f, fmt, arg);
	va_end(arg);
	return n;
}

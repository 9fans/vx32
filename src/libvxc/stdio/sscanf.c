#include <stdio.h>
#include <stdarg.h>

int sscanf(const char *s, const char *fmt, ...)
{
	va_list arg;
	
	va_start(arg, fmt);
	int n = vsscanf(s, fmt, arg);
	va_end(arg);
	return n;
}

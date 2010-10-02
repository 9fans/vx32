#include <stdio.h>
#include <stdarg.h>
#include <string.h>

int vsscanf(const char *s, const char *fmt, va_list arg)
{
	FILE f;
	
	memset(&f, 0, sizeof f);
	f.fd = -1;
	f.isstring = 1;
	f.ibuf = (unsigned char*)s;
	f.ipos = 0;
	f.ilim = strlen(s);
	f.imax = strlen(s);
	
	return vfscanf(&f, fmt, arg);
}

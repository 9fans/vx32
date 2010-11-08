
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include "ioprivate.h"


int vfprintf(FILE *f, const char *fmt, va_list ap)
{
	return vprintfmt((void*)fputc, f, fmt, ap);
}

int fprintf(FILE *f, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int rc = vfprintf(f, fmt, ap);
	va_end(ap);
	return rc;
}

int vprintf(const char *fmt, va_list ap)
{
	return vfprintf(stdout, fmt, ap);
}

int printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int rc = vfprintf(stdout, fmt, ap);
	va_end(ap);
	return rc;
}


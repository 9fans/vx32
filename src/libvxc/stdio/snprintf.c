
#include <stdio.h>
#include <errno.h>

#include "ioprivate.h"

struct snprintbuf {
	char *buf;
	char *ebuf;
};

static int
sprintputch(int ch, struct snprintbuf *b)
{
	if (b->buf < b->ebuf)
		*b->buf++ = ch;
	return 0;
}

int
vsnprintf(char *buf, int n, const char *fmt, va_list ap)
{
	struct snprintbuf b = {buf, buf+n-1};

	if (buf == NULL || n < 1) {
		errno = EINVAL;
		return -1;
	}

	// print the string to the buffer
	int cnt = vprintfmt((void*)sprintputch, &b, fmt, ap);

	// null terminate the buffer
	*b.buf = '\0';

	return cnt;
}

int
snprintf(char *buf, int n, const char *fmt, ...)
{
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = vsnprintf(buf, n, fmt, ap);
	va_end(ap);

	return rc;
}


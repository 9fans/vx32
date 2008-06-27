
#include <stdio.h>
#include <errno.h>

#include "ioprivate.h"

struct sprintbuf {
	char *buf;
};

static int
sprintputch(int ch, struct sprintbuf *b)
{
	*b->buf++ = ch;
	return 0;
}

int
vsprintf(char *buf, const char *fmt, va_list ap)
{
	struct sprintbuf b = {buf};

	if (buf == NULL) {
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
sprintf(char *buf, const char *fmt, ...)
{
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = vsprintf(buf, fmt, ap);
	va_end(ap);

	return rc;
}


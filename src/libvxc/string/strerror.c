#include <errno.h>
#include <stdio.h>
#include <string.h>

#define	UPREFIX		"Unknown error: "

/*
 * Define a buffer size big enough to describe a 64-bit signed integer
 * converted to ASCII decimal (19 bytes), with an optional leading sign
 * (1 byte); finally, we get the prefix and a trailing NUL from UPREFIX.
 */
#define	EBUFSIZE	(20 + sizeof(UPREFIX))

/*
 * Doing this by hand instead of linking with stdio(3) avoids bloat for
 * statically linked binaries.
 */
static void
errstr(int num, char *buf, size_t len)
{
	char *t;
	unsigned int uerr;
	char tmp[EBUFSIZE];

	t = tmp + sizeof(tmp);
	*--t = '\0';
	uerr = (num >= 0) ? num : -num;
	do {
		*--t = "0123456789"[uerr % 10];
	} while (uerr /= 10);
	if (num < 0)
		*--t = '-';
	strlcpy(buf, UPREFIX, len);
	strlcat(buf, t, len);
}

char *
strerror(int num)
{
	static char ebuf[EBUFSIZE];

	if (num > 0 && num < sys_nerr)
		return ((char *)sys_errlist[num]);
	errno = EINVAL;
	errstr(num, ebuf, sizeof(ebuf));
	return (ebuf);
}

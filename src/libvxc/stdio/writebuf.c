
#include <unistd.h>

#include "ioprivate.h"


int __writebuf(FILE *f, const void *buf, size_t size)
{
	const char *lo = buf;
	const char *hi = lo + size;
	while (lo < hi) {
		ssize_t rc = write(f->fd, lo, hi-lo);
		if (rc < 0) {
			f->errflag = 1;
			return EOF;
		}
		lo += rc;
	}
	return 0;
}


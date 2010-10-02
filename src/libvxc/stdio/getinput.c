
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#include "libvxc/private.h"
#include "ioprivate.h"


static char stdinbuf[BUFSIZ];

FILE __stdin = {STDIN_FILENO};


// Prefetch some input data if possible,
// allocating a new input buffer if necessary.
// Assumes the input buffer is currently empty.
int __getinput(FILE *f)
{
	if (f->ibuf == NULL) {
		// No input buffer at all (yet).

		// Give stdin a static buffer,
		// so that we can read stdin without pulling in malloc.
		if (f == stdin) {

			stdin->ibuf = stdinbuf;
			stdin->imax = BUFSIZ;

		} else {

			// File must not be open for writing.
			errno = EBADF;
			f->errflag = 1;
			return EOF;
		}
	}

	// Update offset.
	f->ioffset += f->ilim;
	f->ipos = 0;
	f->ilim = 0;

	// Read some input data
	ssize_t rc = read(f->fd, f->ibuf, f->imax);
	if (rc < 0) {
		f->errflag = 1;
		return EOF;
	}
	if (rc == 0) {
		f->eofflag = 1;
		return EOF;
	}

	f->ipos = 0;
	f->ilim = rc;
	return 0;
}



#include <unistd.h>
#include <errno.h>

#include "libvxc/private.h"
#include "ioprivate.h"


static char stdoutbuf[BUFSIZ];
static char stderrbuf[BUFSIZ];

FILE __stdout = {STDOUT_FILENO, _IOLBF};
FILE __stderr = {STDERR_FILENO, _IOLBF};


static void flushstdout()
{
	fflush(stdout);
	fflush(stderr);
}

// Get some output buffer space,
// allocating a new output buffer or flushing the existing one if necessary.
int __getospace(FILE *f)
{
	if (f->obuf == NULL) {
		// No output buffer at all (yet).

		// Give stdout and stderr static buffers,
		// so that we can get printf without pulling in malloc.
		if (f == stdout || f == stderr) {

			stdout->obuf = stdoutbuf;
			stdout->omax = BUFSIZ;
			stderr->obuf = stderrbuf;
			stderr->omax = BUFSIZ;
			__exit_flush = flushstdout;

		} else {

			// File must not be open for writing.
			errno = EBADF;
			f->errflag = 1;
			return EOF;
		}

	} else {

		// We have an output buffer but it may be full - flush it.
		if (fflush(f) < 0)
			return EOF;
	}

	return 0;
}


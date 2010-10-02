
#include <string.h>
#include <unistd.h>

#include "ioprivate.h"

size_t fread(void *ptr, size_t eltsize, size_t nelts, FILE *f)
{
	size_t totsize = eltsize * nelts;
	size_t actsize = 0;

	while (totsize > 0) {

		size_t buffed = f->ilim - f->ipos;
		if (totsize <= buffed) {

			// The rest of the data we need is in the buffer.
			memcpy(ptr, &f->ibuf[f->ipos], totsize);
			f->ipos += totsize;
			actsize += totsize;
			goto done;
		}

		// Copy any remaining data we may have in the buffer.
		memcpy(ptr, &f->ibuf[f->ipos], buffed);
		f->ipos = f->ilim = 0;
		ptr += buffed;
		actsize += buffed;
		totsize -= buffed;

		// Don't use the buffer for large reads.
		if (totsize >= BUFSIZ) {
			ssize_t rc = read(f->fd, ptr, totsize);
			if (rc < 0) {
				f->errflag = 1;
				goto done;
			}
			if (rc == 0) {
				f->eofflag = 1;
				goto done;
			}
			ptr += rc;
			actsize += rc;
			totsize -= rc;
		} else {
			if (__getinput(f) < 0)
				goto done;
		}
	}

done:
	return actsize / eltsize;
}


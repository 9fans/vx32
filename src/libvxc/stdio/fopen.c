#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "ioprivate.h"

FILE *fopen(const char *name, const char *mode)
{
	int fd, umode, seek, append;
	FILE *f;

	umode = 0;
	seek = 0;
	append = 0;
	if (mode[0] == 'r' && mode[1] == '+') {
		umode = O_RDWR;
	} else if (mode[0] == 'r') {
		umode = O_RDONLY;
	} else if (mode[0] == 'w' && mode[1] == '+') {
		umode = O_RDWR|O_CREAT|O_TRUNC;
	} else if (mode[0] == 'w') {
		umode = O_WRONLY|O_CREAT|O_TRUNC;
	} else if (mode[0] == 'a' && mode[1] == '+') {
		umode = O_RDWR|O_CREAT;
		append = 1;
	} else if (mode[0] == 'a') {
		umode = O_WRONLY|O_CREAT;
		seek = 1;
	} else {
		errno = EINVAL;
		return NULL;
	}

	f = malloc(sizeof *f);
	if (f == NULL)
		return NULL;
	memset(f, 0, sizeof *f);
	f->ibuf = malloc(BUFSIZ);
	f->obuf = malloc(BUFSIZ);
	if (f->ibuf == NULL || f->obuf == NULL) {
	freef:
		free(f->ibuf);
		free(f->obuf);
		free(f);
		return NULL;
	}
	f->imax = BUFSIZ;
	f->omax = BUFSIZ;
	
	if ((fd = open(name, umode, 0666)) < 0)
		goto freef;
		
	if (seek)
		lseek(fd, 0, 2);
	f->append = append;
	f->fd = fd;
	return f;
}

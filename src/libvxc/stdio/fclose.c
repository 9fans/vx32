#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "ioprivate.h"

int fclose(FILE *fin)
{
	if (fin == NULL)
		return -1;
	close(fin->fd);
	free(fin);
	return 0;
}


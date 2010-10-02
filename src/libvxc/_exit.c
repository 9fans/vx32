#include <stdlib.h>
#include <errno.h>
#include "syscall.h"

void _exit(int status)
{
	while(1)
		syscall(VXSYSEXIT, 0, 0, 0, 0, 0);
}


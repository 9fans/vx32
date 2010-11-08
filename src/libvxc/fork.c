#include <unistd.h>
#include <stdlib.h>
#include "syscall.h"

int fork(void)
{
	return syscall(VXSYSFORK, 0, 0, 0, 0, 0);
}


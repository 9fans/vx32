#include <unistd.h>
#include <stdlib.h>
#include "syscall.h"

int sleep(int s)
{
	return syscall(VXSYSSLEEP, s, 0, 0, 0, 0);
}



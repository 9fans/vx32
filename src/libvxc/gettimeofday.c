#include <time.h>
#include <sys/time.h>
#include "syscall.h"

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	return syscall(VXSYSTIME, (int)tv, 0, 0, 0, 0);
}


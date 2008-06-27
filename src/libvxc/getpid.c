#include <unistd.h>
#include <errno.h>
#include "syscall.h"

pid_t getpid(void)
{
	return syscall(VXSYSGETPID, 0, 0, 0, 0, 0);
}


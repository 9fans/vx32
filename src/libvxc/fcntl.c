#include "syscall.h"

int fcntl(int fd, int cmd, int arg)
{
	return syscall(VXSYSFCNTL, fd, cmd, arg, 0, 0);
}


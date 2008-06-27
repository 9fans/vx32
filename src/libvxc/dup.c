#include <unistd.h>
#include "syscall.h"

int dup2(int fd, int nfd)
{
	return syscall(VXSYSDUP, fd, nfd, 0, 0, 0);
}

int dup(int fd)
{
	return dup2(fd, -1);
}


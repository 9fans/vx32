#include <fcntl.h>
#include <errno.h>
#include "syscall.h"

int close(int fd)
{
	return syscall(VXSYSCLOSE, fd, 0, 0, 0, 0);
}

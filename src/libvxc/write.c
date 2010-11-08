#include <unistd.h>
#include <errno.h>
#include "syscall.h"

ssize_t write(int fd, const void *buf, size_t size)
{
	return syscall(VXSYSWRITE, fd, (unsigned)buf, size, 0, 0);
}

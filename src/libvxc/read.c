#include <unistd.h>
#include <errno.h>
#include "syscall.h"

ssize_t read(int fd, void *buf, size_t size)
{
	return syscall(VXSYSREAD, fd, (unsigned)buf, size, 0, 0);
}

#include <errno.h>
#include <unistd.h>
#include "syscall.h"

off_t lseek(int fd, off_t offset, int whence)
{
	return syscall(VXSYSLSEEK, fd, offset, whence, 0, 0);
}


#include <unistd.h>
#include "syscall.h"

int ftruncate(int fd, off_t length)
{
	return syscall(VXSYSFTRUNCATE, fd, length, 0, 0, 0);
}

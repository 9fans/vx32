#include <errno.h>
#include "syscall.h"

int open(const char *path, int flags, int mode)
{
	return syscall(VXSYSOPEN, (int)path, flags, mode, 0, 0);
}

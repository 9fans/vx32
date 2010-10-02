#include <sys/stat.h>
#include "syscall.h"

int fstat(int fd, struct stat *st)
{
	return syscall(VXSYSFSTAT, fd, (int)st, 0, 0, 0);
}


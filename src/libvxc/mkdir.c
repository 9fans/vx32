#include <unistd.h>
#include "syscall.h"

int mkdir(const char *path, mode_t mode)
{
	return syscall(VXSYSMKDIR, (int)path, mode, 0, 0, 0);
}


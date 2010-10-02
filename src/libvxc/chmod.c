#include <unistd.h>
#include "syscall.h"

int chmod(const char *path, mode_t mode)
{
	return syscall(VXSYSCHMOD, (int)path, mode, 0, 0, 0);
}

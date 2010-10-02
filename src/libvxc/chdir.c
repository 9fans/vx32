#include <unistd.h>
#include "syscall.h"

int chdir(const char *path)
{
	return syscall(VXSYSCHDIR, (int)path, 0, 0, 0, 0);
}

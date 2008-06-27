#include <unistd.h>
#include "syscall.h"

int truncate(char *path, off_t length)
{
	return syscall(VXSYSTRUNCATE, (int)path, length, 0, 0, 0);
}

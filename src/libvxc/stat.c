#include <sys/stat.h>
#include "syscall.h"

int stat(const char *path, struct stat *st)
{
	return syscall(VXSYSSTAT, (int)path, (int)st, 0, 0, 0);
}

int lstat(const char *path, struct stat *st)
{
	return syscall(VXSYSLSTAT, (int)path, (int)st, 0, 0, 0);
}

#include <unistd.h>
#include "syscall.h"

int link(const char *old, const char *new)
{
	return syscall(VXSYSLINK, (int)old, (int)new, 0, 0, 0);
}


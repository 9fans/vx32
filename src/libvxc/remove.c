#include "syscall.h"

int remove(const char *name)
{
	return syscall(VXSYSREMOVE, (int)name, 0, 0, 0, 0);
}

int unlink(const char *name)
{
	return remove(name);
}

int rmdir(const char *name)
{	
	return remove(name);
}

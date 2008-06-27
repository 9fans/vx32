#include <stdio.h>
#include "syscall.h"

char *getcwd(char *buf, int size)
{
	char *s = (char*)syscall(VXSYSGETCWD, (int)buf, size, 0, 0, 0);
	if(s == (char*)-1)
		return NULL;
	return s;
}


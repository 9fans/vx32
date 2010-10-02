#include <unistd.h>
#include <errno.h>
#include "syscall.h"

extern void end;

static void *brk = &end;

void *sbrk(intptr_t increment)
{
	void *oldbrk = brk;
	void *newbrk = (char*)brk + increment;
	int ret = syscall(VXSYSBRK, (unsigned)newbrk, 0, 0, 0, 0);
	if(ret == -1)
		return (void*)-1;
	brk = newbrk;
	return oldbrk;
}


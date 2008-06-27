#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "syscall.h"

pid_t waitpid(pid_t wpid, int *status, int options)
{
	return syscall(VXSYSWAITPID, wpid, (int)status, options, 0, 0);
}

#include <unistd.h>
#include "syscall.h"

int pipe(int fd[2])
{
	return syscall(VXSYSPIPE, (int)fd, 0, 0, 0, 0);
}

#include <sys/select.h>
#include "syscall.h"

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	 struct timeval *timeout)
{
	return syscall(VXSYSSELECT, nfds, (int)readfds, (int)writefds, (int)exceptfds, (int)timeout);
}


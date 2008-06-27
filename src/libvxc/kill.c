#include <unistd.h>
#include <errno.h>

int kill(pid_t pid, int sig)
{
	errno = EINVAL;
	return -1;
}


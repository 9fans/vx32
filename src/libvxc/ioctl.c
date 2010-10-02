#include <unistd.h>
#include <errno.h>

int ioctl(int fd, int cmd, ...)
{
	errno = EINVAL;
	return -1;
}


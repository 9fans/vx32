#include <unistd.h>
#include "rep.h"

char buf[65536];

int main()
{
	int fd = open("/dev/null", 1);
	if (fd < 0)
		return -1;
	for (int i = 0; i < 1000000; i++) {
		write(fd, buf, sizeof(buf));
	}
	return 0;
}


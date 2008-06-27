#include <unistd.h>
#include "rep.h"

char buf[65536];

int main()
{
	int fd = open("/dev/zero", 0);
	if (fd < 0)
		return -1;
	for (int i = 0; i < 100000; i++) {
		read(fd, buf, sizeof(buf));
	}
	return 0;
}


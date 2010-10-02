#include <time.h>
#include <sys/time.h>
#include "syscall.h"

time_t time(time_t *a)
{
	struct timeval tv;
	if(gettimeofday(&tv, NULL) < 0)
		return -1;
	int t = tv.tv_sec;
	if (a)
		*a = t;
	return t;
}

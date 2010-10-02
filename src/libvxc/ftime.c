#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>

int ftime(struct timeb *tb)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	tb->time = tv.tv_sec;
	tb->millitm = tv.tv_usec / 1000;
	tb->timezone = 0;
	tb->dstflag = 0;
	return 0;
}


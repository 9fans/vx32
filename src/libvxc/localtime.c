#include <time.h>

static int mday[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

struct tm *localtime(const time_t *tp)
{
	time_t t = *tp;
	static struct tm tm;
	
	tm.tm_sec = t % 60;
	t /= 60;
	tm.tm_min = t % 60;
	t /= 60;
	tm.tm_hour = t % 24;
	t /= 24;
	tm.tm_wday = (t + 4) % 7;  // Jan 1 1970 = Thursday
	
	int isleap = 0;
	for (tm.tm_year=70;; tm.tm_year++) {
		isleap = (tm.tm_year+1900)%4 == 0;
		if (t < 365 + isleap)
			break;
		t -= 365 + isleap;
	}
	
	tm.tm_yday = t;
	for (tm.tm_mon=0;; tm.tm_mon++) {
		isleap = (tm.tm_year+1900)%4 == 0 && tm.tm_mon == 1;
		if (t < mday[tm.tm_mon] + isleap)
			break;
		t -= mday[tm.tm_mon] + isleap;
	}
	tm.tm_mday = t + 1;
	tm.tm_isdst = 0;

	return &tm;
}

struct tm *gmtime(const time_t *tp)
{
	return localtime(tp);
}

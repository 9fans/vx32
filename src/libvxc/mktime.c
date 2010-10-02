#include <time.h>

static int mday[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

time_t mktime(const struct tm *tm)
{
	int i;
	time_t t = 0;
	for(i=70; i<tm->tm_year; i++){
		if(i%4 == 0)
			t += 86400 * 366;
		else
			t += 86400 * 365;
	}
	for(i=0; i<tm->tm_mon; i++){
		t += mday[i] * 86400;
		if (tm->tm_year % 4 == 0 && i == 2)
			t += 86400;
	}
	t += (tm->tm_mday-1) * 86400;
	t += tm->tm_hour * 3600;
	t += tm->tm_min * 60;
	t += tm->tm_sec;
	return t;
}

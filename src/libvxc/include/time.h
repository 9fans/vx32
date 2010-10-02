#ifndef _TIME_H
#define _TIME_H

#include <sys/types.h>


struct tm {
	int    tm_sec;
	int    tm_min;
	int    tm_hour;
	int    tm_mday;
	int    tm_mon;
	int    tm_year;
	int    tm_wday;
	int    tm_yday;
	int    tm_isdst;
};


// Time conversion
char *asctime(const struct tm *);
char *asctime_r(const struct tm *__restrict, char *__restrict);

char *ctime(const time_t *);
char *ctime_r(const time_t *, char *);

// Time conversion using format string
size_t strftime(char *__restrict s, size_t maxsize,
	const char *__restrict format, const struct tm *__restrict timeptr);
char *strptime(const char *__restrict buf, const char *__restrict format,
	struct tm *__restrict tm);

// Time arithmetic
double difftime(time_t, time_t);

// Time breakdown into struct tm
struct tm *gmtime(const time_t *);
struct tm *gmtime_r(const time_t *__restrict, struct tm *__restrict);
struct tm *localtime(const time_t *);
struct tm *localtime_r(const time_t *__restrict, struct tm *__restrict);
time_t mktime(const struct tm *);

// Current wall-clock time
time_t time(time_t*);

#define CLOCKS_PER_SEC 128
// Current virtual CPU usage counter
clock_t clock(void);

#endif	// _TIME_H

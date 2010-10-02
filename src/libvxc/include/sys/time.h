#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <sys/types.h>

struct timeval {
	time_t		tv_sec;		// Seconds
	suseconds_t	tv_usec;	// Microseconds
};

struct itimerval {
	struct timeval	it_interval;	// Timer interval
	struct timeval	it_value;	// Current value
};
struct timezone;

int gettimeofday(struct timeval *__restrict, struct timezone *__restrict);

#endif	// _SYS_TIME_H

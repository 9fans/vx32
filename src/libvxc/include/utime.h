#ifndef _UTIME_H_ 
#define _UTIME_H_

#include <time.h>

struct utimbuf {
	time_t actime;
	time_t modtime;
}; 

int utime(const char *path, const struct utimbuf*);

#endif

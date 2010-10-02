
#include <unistd.h>

#include "rep.h"

int main()
{
	for (int i = 0; i < 10000; i++) {
//		REP100(getpid();)	// Mac OS X caches in libc
		REP100(close(-1);)
//		REP100(sleep(0);)
	}
	return 0;
}


#include <time.h>
#include "syscall.h"

clock_t clock(void)
{
	return syscall(VXSYSCLOCK, 0, 0, 0, 0, 0);
}

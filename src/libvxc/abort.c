#include <stdlib.h>

void abort()
{
	exit(1);	// No signals
}

void __stack_chk_fail(void) { }

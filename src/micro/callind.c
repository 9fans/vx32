
#include "rep.h"

volatile void foo();

int main()
{
	for (int i = 0; i < 10000000; i++) {
		REP100(asm volatile("call *%0" : : "r" (foo));)
	}
	return 0;
}

volatile void foo()
{
}


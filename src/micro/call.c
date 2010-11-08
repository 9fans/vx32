
#include "rep.h"

volatile void foo();

int main()
{
	for (int i = 0; i < 10000000; i++) {
		REP100(asm volatile("call foo");)
	}
	return 0;
}

asm(".text; foo: ret");


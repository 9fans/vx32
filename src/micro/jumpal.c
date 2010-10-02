
#include "rep.h"

int main()
{
	for (int i = 0; i < 100000000; i++) {
		REP100(asm volatile("jmp 1f; .p2align 4; 1:");)
							// aligned target
	}
	return 0;
}


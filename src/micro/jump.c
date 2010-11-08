
#include "rep.h"

int main()
{
	for (int i = 0; i < 100000000; i++) {
		REP100(asm volatile("jmp 1f; 1:");)	// unaligned target
	}
	return 0;
}


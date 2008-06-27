#include "rep.h"

int main()
{
	for (int i = 0; i < 10000000; i++) {
		REP100(asm volatile("jmp 1f; .p2align 12; 1:");)  // too-aligned target
	}
	return 0;
}


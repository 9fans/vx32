#include "rep.h"

volatile void foo(long long);
volatile long long a = 0x123456789abcdefLL;
volatile long long b = 0xfedcba987654321LL;

int main()
{
	long long a1 = a;
	long long b1 = b;
	for (int i = 0; i < 1000000; i++) {
		REP100(a /= b; a+=b;)
	}
	asm volatile("" : : "r" (a) : "memory");  // fake out optimizer
	return 0;
}


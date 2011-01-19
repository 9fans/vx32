#include <libkern/OSAtomic.h>

int
oscmpswap(long *addr, long oldValue, long newValue)
{
	if (OSAtomicCompareAndSwapLong(oldValue, newValue, addr))
		return 1;
	return 0;
}

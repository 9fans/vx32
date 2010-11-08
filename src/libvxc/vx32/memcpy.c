
#include <string.h>
#include <stdint.h>

void *memcpy(void *dest, const void *s, size_t n)
{
	void *d = dest;
	uint32_t nn;

	// It's not worth trying to optimize really short copies.
	if (n < 16)
		goto bytecopy;

	// Try to get the dest and src pointers on a 32-byte boundary
	if ((uint32_t)d & 3) {

		if (((uint32_t)d & 3) != ((uint32_t)s & 3))
			goto bytecopy;

		nn = 4 - ((uint32_t)d & 3);
		if (nn > n)
			nn = n;

		asm volatile("rep movsb"
				: "=D" (d), "=S" (s)
				: "D" (d), "S" (s), "c" (nn)
				: "memory");
	}

	// Copy 32-bit words
	if (n >> 2) {
		asm volatile("rep movsl"
				: "=D" (d), "=S" (s)
				: "D" (d), "S" (s), "c" (n >> 2)
				: "memory");
		n &= 3;
	}

	// Copy any remaining bytes
	bytecopy:
	if (n > 0) {
		asm volatile("rep movsb"
				: "=D" (d), "=S" (s)
				: "D" (d), "S" (s), "c" (n)
				: "memory");
	}

	return dest;
}


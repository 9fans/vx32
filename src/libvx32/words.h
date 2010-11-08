//
// Word access and byte-swapping functions specific to x86-32
//
#ifndef X86_32_WORDS_H
#define X86_32_WORDS_H

#include <inttypes.h>


// Efficient byte-swap functions for x86-32
static inline uint16_t
bswap16(uint16_t v) {
	asm("xchgb %%al,%%ah" : "=a" (v) : "a" (v));
	return v;
}

static inline uint32_t
bswap32(uint32_t v) {
	asm("bswapl %%eax" : "=a" (v) : "a" (v));
	return v;
}

static inline uint64_t
bswap64(uint64_t v) {
	return ((uint64_t)bswap32(v) << 32) | bswap32(v >> 32);
}


// Utility macros/functions for converting
// between host byte order and little-endian VX32 byte order.
// The x86 is little-endian already, so these are no-ops.
#define htol16(x)	(x)
#define ltoh16(x)	(x)
#define htol32(x)	(x)
#define ltoh32(x)	(x)
#define htol64(x)	(x)
#define ltoh64(x)	(x)


// Utility macros/functions for converting
// between host byte order and big-endian ("network") byte order.
#define htob16(x)	bswap16(x)
#define btoh16(x)	bswap16(x)
#define htob32(x)	bswap32(x)
#define btoh32(x)	bswap32(x)
#define htob64(x)	bswap64(x)
#define btoh64(x)	bswap64(x)


// Macros to access unaligned words in memory - trivial on the x86
#define getu16(p)	(*(uint16_t*)(p))		// host byte order
#define getu32(p)	(*(uint32_t*)(p))
#define getu64(p)	(*(uint64_t*)(p))
#define getu16l(p)	getu16(p)			// little-endian
#define getu32l(p)	getu32(p)
#define getu64l(p)	getu64(p)
#define getu16n(p)	bswap16(getu16(p))		// big-endian
#define getu32n(p)	bswap32(getu32(p))
#define getu64n(p)	bswap64(getu64(p))

#define putu16(p, v)	(*(uint16_t*)(p) = (v))		// host byte order
#define putu32(p, v)	(*(uint32_t*)(p) = (v))
#define putu64(p, v)	(*(uint64_t*)(p) = (v))
#define putu16l(p, v)	putu16((p), (v))		// little-endian
#define putu32l(p, v)	putu32((p), (v))
#define putu64l(p, v)	putu64((p), (v))
#define putu16n(p, v)	putu16((p), bswap16(v))		// big-endian
#define putu32n(p, v)	putu32((p), bswap32(v))
#define putu64n(p, v)	putu64((p), bswap64(v))


#endif	// X86_32_WORDS_H

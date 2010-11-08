#ifndef _ALLOCA_H
#define _ALLOCA_H

/* Use GCC's built-in dynamic stack allocator */
#define alloca(size) __builtin_alloca (size)

#endif	// _ALLOCA_H

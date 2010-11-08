#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef NDEBUG
#define assert(v)	((void) 0)
#else
#define assert(v)	((v) ? (void)0 : __assert_fail(__FILE__, __LINE__, \
							__func__, #v))
#endif

extern void __assert_fail(const char *file, int line,
			const char *func, const char *exp);

#endif	// _ASSERT_H

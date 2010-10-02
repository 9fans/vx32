#undef _FORTIFY_SOURCE	/* stupid ubuntu setting that warns about not checking the return value from write */
#define _BSD_SOURCE 1
#define _NETBSD_SOURCE 1	/* NetBSD */
#define _SVID_SOURCE 1
#if !defined(__APPLE__) && !defined(__OpenBSD__)
#	define _XOPEN_SOURCE 1000
#	define _XOPEN_SOURCE_EXTENDED 1
#endif
#if defined(__FreeBSD__)
#	include <sys/cdefs.h>
	/* for strtoll */
#	undef __ISO_C_VISIBLE
#	define __ISO_C_VISIBLE 1999
#	undef __LONG_LONG_SUPPORTED
#	define __LONG_LONG_SUPPORTED
#	undef __BSD_VISIBLE
#	define __BSD_VISIBLE 1
#endif
#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <setjmp.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

#define ushort _ushort
#define uint _uint
#define ulong _ulong
#define uchar _uchar

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long uvlong;
typedef long long vlong;
typedef ulong uintptr;
typedef signed char schar;

typedef unsigned short Rune;

typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef int int32;
typedef long long int64;

#define USED(x) ((void)(x))

void plimit(pid_t, int);

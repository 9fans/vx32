#include <errno.h>

static inline int
syscall(int num, unsigned a1, unsigned a2, unsigned a3, unsigned a4, unsigned a5)
{
	int ret;

	asm volatile("syscall\n"
		: "=a" (ret)
		: "a" (num),
		  "d" (a1),
		  "c" (a2),
		  "b" (a3),
		  "D" (a4),
		  "S" (a5)
		: "cc", "memory");
	
	if(ret < 0){
		errno = -ret;
		return -1;
	}
	return ret;
}

enum
{
	VXSYSEXIT = 1,
	VXSYSBRK = 2,
	VXSYSREAD = 3,
	VXSYSWRITE = 4,
	VXSYSOPEN = 5,
	VXSYSCLOSE = 6,
	VXSYSLSEEK = 7,
	VXSYSREMOVE = 8,
	VXSYSTIME = 9,	// gettimeofday
	VXSYSCLOCK = 10,
	VXSYSSTAT = 11,
	VXSYSFSTAT = 12,
	VXSYSGETCWD = 13,
	VXSYSCHDIR = 14,
	VXSYSCHMOD = 15,
	VXSYSDUP = 16,
	VXSYSLINK = 17,
	VXSYSSELECT = 18,
	VXSYSMKDIR = 19,
	VXSYSFCNTL = 20,
	VXSYSTRUNCATE = 21,
	VXSYSFTRUNCATE = 22,
	VXSYSLSTAT = 23,
	VXSYSFORK = 24,
	VXSYSWAITPID = 25,
	VXSYSEXEC = 26,
	VXSYSPIPE = 27,
	VXSYSSLEEP = 28,
	VXSYSGETPID = 29,
};


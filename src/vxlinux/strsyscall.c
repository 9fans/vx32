#include <stdio.h>
#include <asm/unistd.h>

static char *syscall_names[] = {
};


#define nelem(x) (sizeof(x)/sizeof((x)[0]))

char *strsyscall(int n) {
	if (0 <= n && n < nelem(syscall_names) && syscall_names[n])
		return syscall_names[n];
	static char buf[40];
	snprintf(buf, sizeof buf, "syscall%#x", n);
	return buf;
}


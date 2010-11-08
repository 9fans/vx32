#define _GNU_SOURCE // for syscall in unistd.h
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include "vx32.h"

#define nelem(x) (sizeof(x)/sizeof((x)[0]))

extern char *strsyscall(int);  // strsyscall.c

static const char *progname;

static void fatal(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "%s: fatal error: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(2);
}

static void dumpregs(struct vxproc *p)
{
	struct vxcpu *c = p->cpu;

	fprintf(stderr, "eax %08x  ecx %08x  edx %08x  ebx %08x\n",
		c->reg[EAX], c->reg[ECX], c->reg[EDX], c->reg[EBX]);
	fprintf(stderr, "esp %08x  ebp %08x  esi %08x  edi %08x\n",
		c->reg[ESP], c->reg[EBP], c->reg[ESI], c->reg[EDI]);
	fprintf(stderr, "eip %08x  eflags %08x\n",
		c->eip, c->eflags);

//	for (int i = 0; i < 8; i++) {
//		int32_t *val = r.xmm[i].i32;
//		fprintf(stderr, "xmm%d %08x%08x%08x%08x\n",
//			i, val[3], val[2], val[1], val[0]);
//	}
}

int trace;

// macros for use within system calls
#define NUM proc->cpu->reg[EAX]
#define ARG1 proc->cpu->reg[EBX]
#define ARG2 proc->cpu->reg[ECX]
#define ARG3 proc->cpu->reg[EDX]
#define ARG4 proc->cpu->reg[ESI]
#define ARG5 proc->cpu->reg[EDI]

#define SYSCALL(x) static int x(vxproc *proc, vxmmap *m)

// Translate pointer - guest to host
#define TX(a) ((a) ? (a)+m->base : 0)

SYSCALL(sys0) { return syscall(NUM); }
SYSCALL(sys1I) { return syscall(NUM, ARG1); }
SYSCALL(sys1P) { return syscall(NUM, TX(ARG1)); }
SYSCALL(sys2II) { return syscall(NUM, ARG1, ARG2); }
SYSCALL(sys2IP) { return syscall(NUM, ARG1, TX(ARG2)); }
SYSCALL(sys2PI) { return syscall(NUM, TX(ARG1), ARG2); }
SYSCALL(sys2PP) { return syscall(NUM, TX(ARG1), TX(ARG2)); }
SYSCALL(sys3IIP) { return syscall(NUM, ARG1, ARG2, TX(ARG3)); }
SYSCALL(sys3III) { return syscall(NUM, ARG1, ARG2, ARG3); }
SYSCALL(sys3PII) { return syscall(NUM, TX(ARG1), ARG2, ARG3); }
SYSCALL(sys3PPI) { return syscall(NUM, TX(ARG1), TX(ARG2), ARG3); }
SYSCALL(sys3IPI) { return syscall(NUM, ARG1, TX(ARG2), ARG3); }
SYSCALL(sys4IPPI) { return syscall(NUM, ARG1, TX(ARG2), TX(ARG3), ARG4); }
SYSCALL(sys5IIIPI) { return syscall(NUM, ARG1, ARG2, ARG3, TX(ARG4), ARG5); }

// Linux brk doesn't follow the usual system call conventions.
// It returns the new brk on success, the old one on failure.
SYSCALL(sysbrk)
{
	uint32_t oaddr = m->size;
	uint32_t addr = ARG1;
	if (addr == 0)
		return oaddr;
	if (addr == oaddr)
		return oaddr;
	if (vxmem_resize(proc->mem, addr) < 0)
		return oaddr;
	if (addr > oaddr)
		vxmem_setperm(proc->mem, oaddr, addr - oaddr, VXPERM_READ|VXPERM_WRITE);
	return addr;
}

SYSCALL(sysinval)
{
	errno = EINVAL;
	return -1;
}

SYSCALL(sysfcntl64)
{
	switch (ARG1) {
	default:
		errno = EINVAL;
		return -1;

	case F_DUPFD:
	case F_GETFD:
	case F_SETFD:
	case F_GETFL:
	case F_SETFL:
	case F_GETOWN:
	case F_SETOWN:
	case F_GETSIG:
	case F_SETSIG:
	case F_GETLEASE:
	case F_SETLEASE:
	case F_NOTIFY:
		return syscall(NUM, ARG1, ARG2, ARG3);
	
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		return syscall(NUM, ARG1, ARG2, TX(ARG3));
	}
}

static int (*syscalls[])(vxproc*, vxmmap*) =
{
	[SYS_brk]           sysbrk,
	[SYS_close]         sys1I,
	[SYS_chmod]         sys2PI,
	[SYS_chown]         sys3PII,
	[SYS_creat]         sys2PI,
	[SYS_dup]           sys1I,
	[SYS_dup2]          sys2II,
	[SYS_exit_group]    sys1I,
	[SYS_fcntl64]       sysfcntl64,
	[SYS_fchmod]        sys2II,
	[SYS_fchown]        sys3III,
	[SYS_fstat64]       sys2IP,
	[SYS_getegid32]     sys0,
	[SYS_geteuid32]     sys0,
	[SYS_getgid32]      sys0,
	[SYS_getpid]        sys0,
	[SYS_getuid32]      sys0,
	[SYS_ioctl]         sys3IIP,
	[SYS__llseek]       sys5IIIPI,
	[SYS_lchown]        sys3PII,
	[SYS_link]          sys2PP,
	[SYS_lseek]         sys3III,
	[SYS_mkdir]         sys2PI,
	[SYS_mmap]          sysinval,
	[SYS_mmap2]         sysinval,
	[SYS_open]          sys3PII,
	[SYS_read]          sys3IPI,
	[SYS_readlink]      sys3PPI,
	[SYS_rmdir]         sys1P,
	[SYS_rt_sigaction]  sys4IPPI,
	[SYS_stat64]        sys2PP,
	[SYS_symlink]       sys2PP,
	[SYS_time]          sys1P,
	[SYS_uname]         sys1P,
	[SYS_unlink]        sys1P,
	[SYS_write]         sys3IPI,
};

static void dosyscall(vxproc *proc)
{
	int n = NUM;
	if (trace)
		fprintf(stderr, "%s %08x %08x %08x %08x %08x\n", 
			strsyscall(NUM), ARG1, ARG2, ARG3, ARG4, ARG5);
	if (n < nelem(syscalls) && n >= 0 && syscalls[n]) {
		vxmmap *m = vxmem_map(proc->mem, 0);
		int ret = syscalls[n](proc, m);
		if (n != SYS_brk && ret < 0)
			ret = -errno;
		vxmem_unmap(proc->mem, m);
		proc->cpu->reg[EAX] = ret;
		return;
	}
	dumpregs(proc);
	fatal("syscall not implemented - %s", strsyscall(NUM));
}

extern char **environ;

int main(int argc, const char *const *argv)
{
	int i;
	progname = argv[0];

	if (argc > 1 && strcmp(argv[1], "-t") == 0){
		argc--;
		argv++;
		trace++;
	}

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <vx-program> <args>\n",
			progname);
		exit(1);
	}
	const char *loadname = argv[1];

	FILE *f = fopen("/dev/tty", "w");
	if(f){
		char buf[1000];
		if(getcwd(buf, sizeof buf) != NULL)
			fprintf(f, "cd %s\n", buf);
		fprintf(f, "vxlinux");
		for(i=1; i<argc; i++)
			fprintf(f, " %s", argv[i]);
		fprintf(f, "\n");
	}

	vxproc *p = vxproc_alloc();
	if (p == NULL)
		fatal("vxproc_new: %s\n", strerror(errno));
	p->allowfp = 1;

	if (vxproc_loadelffile(p, loadname, &argv[1], (const char**)environ) < 0)
		fatal("vxproc_loadfile: %s\n", strerror(errno));

	vx32_siginit();
	dumpregs(p);

	// Simple execution loop.
	for (;;) {
		int rc = vxproc_run(p);
		if (rc < 0)
			fatal("vxproc_run: %s\n", strerror(errno));
		if (rc == VXTRAP_SOFT + 0x80) {
			dosyscall(p);
			continue;
		}
		dumpregs(p);
		fatal("vxproc_run trap %#x\n", rc);
	}
	return 0;	// not reached
}


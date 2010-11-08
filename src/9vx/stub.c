
#define	WANT_M

#ifdef __APPLE__
#define __DARWIN_UNIX03 0
#endif

#include	"u.h"
#include	<sched.h>
#include	<signal.h>
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"error.h"

int tracefp;

/*
 * Simple xalloc memory allocator.
 */
static int xalloced;

void*
xallocz(ulong size, int zero)
{
	void *v;
	
	v = malloc(size);
	if(v && zero)
		memset(v, 0, size);
	if(v)
		xalloced += size;
	return v;
}

void*
xalloc(ulong size)
{
	return xallocz(size, 1);
}

void
xfree(void *p)
{
	free(p);
}

void
xsummary(void)
{
	print("%d allocated\n", xalloced);
}

/* 
 * Very simple non-caching cache implementation.
 */
void
cinit(void)
{
}

void
copen(Chan *c)
{
	USED(c);
}

int
cread(Chan *c, uchar *buf, int len, vlong off)
{
	USED(c);
	USED(buf);
	USED(len);
	USED(off);

	return 0;
}

void
cupdate(Chan *c, uchar *buf, int len, vlong off)
{
	USED(c);
	USED(buf);
	USED(len);
	USED(off);
}

void
cwrite(Chan* c, uchar *buf, int len, vlong off)
{
	USED(c);
	USED(buf);
	USED(len);
	USED(off);
}

/*
 * Interrupt priority level
 */
int
_splx(int s)
{
	int ospl;
	
	ospl = m->spl;
	m->spl = s;
	return ospl;
}

int
splhi(void)
{
	return _splx(1);
}

int
spllo(void)
{
	return _splx(0);
}

int
islo(void)
{
	return m->spl == 0;
}

void
splx(int s)
{
	_splx(s);
}


/*
 * Floating point.
 */
void
fpoff(void)
{
}

void
fpinit(void)
{
	if(tracefp)
		iprint("fpinit\n");

#ifdef i386
	asm volatile(
		"finit\n"
		"fwait\n"
		"pushw $0x232\n"
		"fldcw 0(%%esp)\n"
		"popw %%ax\n"
		"fwait\n" : : : "memory");
#else
	asm volatile(
		"finit\n"
		"fwait\n"
		"pushq $0x232\n"
		"fldcw 0(%%rsp)\n"
		"popq %%rax\n"
		"fwait\n" : : : "memory");
#endif

}

void
fpsave(FPsave *s)
{
#ifdef i386
	asm volatile("fnsave 0(%%eax)\n" : : "a" (s) : "memory");
#else
	asm volatile("fnsave 0(%%rax)\n" : : "a" (s) : "memory");
#endif
	if(tracefp)
		iprint("fpsave: %#x %#x %#x %#ux\n", s->control, s->status, s->tag, s->pc);
}

void
fprestore(FPsave *s)
{
	if(tracefp)
		iprint("fprestore: %#x %#x %#x %#ux\n", s->control, s->status, s->tag, s->pc);
#ifdef i386
	asm volatile("frstor 0(%%eax); fwait\n" : : "a" (s) : "memory");
#else
	asm volatile("frstor 0(%%rax); fwait\n" : : "a" (s) : "memory");
#endif
}

void
fpenv(FPsave *s)
{
	if(tracefp)
		iprint("fpenv: %#x %#x %#x %#ux\n", s->control, s->status, s->tag, s->pc);
#ifdef i386
	asm volatile("fstenv 0(%%eax)\n" : : "a" (s) : "memory");
#else
	asm volatile("fstenv 0(%%rax)\n" : : "a" (s) : "memory");
#endif
}

void
fpclear(void)
{
	if(tracefp)
		iprint("fpclear\n");
	asm volatile("fclex\n");
}

ulong
fpstatus(void)
{
	ushort x;
	asm volatile("fstsw %%ax\n" : "=a" (x));
	return x;
}


/*
 * Malloc (#defined to _kmalloc) zeros its memory.
 */
void*
malloc(ulong size)
{
	return calloc(1, size);
}

void
mallocsummary(void)
{
}

void
setmalloctag(void *v, ulong tag)
{
}

void*
smalloc(ulong size)
{
	void *v;

	for(;;){
		v = malloc(size);
		if(v != nil){
			memset(v, 0, size);  // XXX
			return v;
		}
		tsleep(&up->sleep, return0, 0, 100);
	}
}


#undef malloc
void*
mallocz(ulong size, int clr)
{
	if(clr)
		return calloc(1, size);
	else
		return malloc(size);
}
#define malloc _kmalloc


/*
 * Spin locks
 */
int
tas(void *x)
{
	int     v;

#ifdef i386
	__asm__(	"movl   $1, %%eax\n\t"
			"xchgl  %%eax,(%%ecx)"
			: "=a" (v)
			: "c" (x)
	);
#else
	__asm__(	"movl   $1, %%eax\n\t"
			"xchgl  %%eax,(%%rcx)"
			: "=a" (v)
			: "c" (x)
	);
#endif

	switch(v) {
	case 0:
	case 1:
		return v;
	default:
		print("tas: corrupted lock 0x%lux\n", v);
		return 1;
	}
}

int
_tas(void *x)
{
	return tas(x);
}

int
lock(Lock *lk)
{
	int i, j, printed;
	
	for(i=0; i<1000; i++){
		if(canlock(lk))
			return 1;
		sched_yield();
	}
	for(j=10; j<=1000; j*=10)
		for(i=0; i<10; i++){
			if(canlock(lk))
				return 1;
			microdelay(j);
		}
	printed = 0;
	for(;;){
		if(canlock(lk))
			return 1;
		if(!printed++)
			iprint("cpu%d deadlock? %p caller=%p\n",
				m->machno, lk, getcallerpc(&lk));
		microdelay(10000);
	}
	return 0;
}

void
unlock(Lock *l)
{
	if(l->key == 0)
		iprint("unlock: not locked: pc %luX\n",
			getcallerpc(&l));
	if(l->isilock)
		iprint("unlock of ilock: pc %lux, held by %lux\n",
			getcallerpc(&l), l->pc);
	if(l->p != up)
		iprint("unlock: up changed: pc %lux, acquired at pc %lux, lock p 0x%p, unlock up 0x%p\n",
			getcallerpc(&l), l->pc, l->p, up);
	l->m_ = nil;
	if(up)
		up->nlocks.ref--;
	l->key = 0;
}

int
canlock(Lock *l)
{
	if(up)
		up->nlocks.ref++;
	if(tas(&l->key)){
		if(up)
			up->nlocks.ref--;
		return 0;
	}

	if(up)
		up->lastlock = l;
	l->pc = getcallerpc(&l);
	l->p = up;
	l->m_ = MACHP(m->machno);
	l->isilock = 0;
	return 1;
}

void
ilock(Lock *lk)
{
	int s;
	
	s = splhi();
	lock(lk);
	lk->sr = s;
}

void
iunlock(Lock *lk)
{
	int s;
	
	s = lk->sr;
	unlock(lk);
	splx(s);
}


/*
 * One of a kind
 */
#include "kerndate.h"

ulong
getcallerpc(void *v)
{
	return ((ulong*)v)[-1];
}

static int randfd = -1;
void
randominit(void)
{
	if((randfd = open("/dev/urandom", OREAD)) < 0)
	if((randfd = open("/dev/random", OREAD)) < 0)
		panic("open /dev/random: %r");
}

ulong
randomread(void *v, ulong n)
{
	int r;

	if(randfd < 0)
		randominit();
	if((r = read(randfd, v, n)) != n)
		panic("short read from /dev/random: %d but %d", n, r);
	return r;
}

int cpuserver = 0;

void
rebootcmd(int argc, char **argv)
{
	int i;
	restoretty();
	for(i = 0; i < argc; i++)
		iprint("%s%s", argv[i], argc - i > 1 ? " " : "");
	if(argc > 0)
		iprint("\n");
	exit(0);
	error(Egreg);
}

void
labelinit(Label *l, ulong pc, ulong sp)
{
	assert(l);
	setlabel(l);
	l->pc = pc;

	/*
	 * Stack pointer at call instruction (before return address
	 * gets pushed) must be 16-byte aligned.
	 */
	if((uintptr)sp%4)
		panic("labelinit %#lux %#lux", pc, sp);
	while((uintptr)sp%64)
		sp -= 4;
	sp -= 8;	// trial and error on OS X
	l->sp = sp;
//iprint("labelinit %p %p\n", pc, sp);
}

void
dumpstack(void)
{
}

void
rdb(void)
{
}

void
halt(void)
{
}

void
checkmmu(ulong a, ulong b)
{
}

void
delay(int x)
{
	// no
}

void
reboot(void *entry, void *code, ulong size)
{
	restoretty(); exit(0);
	error(Egreg);
}

void
countpagerefs(ulong *ref, int print)
{
	panic("countpagerefs");
}


/*
 * Debugging prints go to standard error, always.
 */
int
iprint(char *fmt, ...)
{
	int n;
	va_list arg;
	char buf[PRINTSIZE];

	va_start(arg, fmt);
	n = vseprint(buf, buf+sizeof(buf), fmt, arg) - buf;
	va_end(arg);
	write(2, buf, n);
	return n;
}

void 
talktome(void)
{
	int i;
	static char cmd[512];
	while (fgets(cmd, sizeof(cmd), stdin)) {
		if (! strcmp(cmd, "mach")) {
			for(i = 0; i < MAXMACH; i++) {
				fprintf(stderr, "%d ", MACHP(i)->splpc);
			}
			
		}
	}
	fprintf(stderr, "We're done talking\n");
}
/*
 * Panics go to standard error.
 */
int panicking;
void
panic(char *fmt, ...)
{
	int n;
	va_list arg;
	char buf[PRINTSIZE];

	if(panicking)
		for(;;);
	panicking = 1;

	strcpy(buf, "9vx panic: ");
	va_start(arg, fmt);
	n = vseprint(buf+strlen(buf), buf+sizeof(buf), fmt, arg) - buf;
	va_end(arg);
	buf[n] = '\n';
	write(2, buf, n+1);
	restoretty();
	if(doabort){
#ifdef __APPLE__
		fprint(2, "sleeping, so you can attach gdb to pid %d\n", (int)getpid());
		for(;;)
			microdelay(1000000);
#else
		fprint(2, "aborting, to dump core.\n");
		talktome();
		abort();
#endif
	}
	exit(0);
}

/*
 * Sleazy: replace vsnprintf with vsnprint, so that
 * vxprint will use the Fmt library, which behaves
 * better on small stacks.
 *
 * TODO: Apple linker doesn't like this.
 */
#ifndef __APPLE__
int
vsnprintf(char *buf, size_t nbuf, const char *fmt, va_list arg)
{
	return vsnprint(buf, nbuf, (char*)fmt, arg);
}
#endif

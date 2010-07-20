#include "portfns.h"

void	aamloop(int);
Dirtab*	addarchfile(char*, int, long(*)(Chan*,void*,long,vlong), long(*)(Chan*,void*,long,vlong));
void	archinit(void);
void	bootargs(void*);
ulong	cankaddr(ulong);
void	clockintr(Ureg*, void*);
int	(*cmpswap)(long*, long, long);
int	cmpswap486(long*, long, long);
void	(*coherence)(void);
void	cpuid(int, ulong regs[]);
int	cpuidentify(void);
void	cpuidprint(void);
void	(*cycles)(uvlong*);
void	delay(int);
int	dmacount(int);
int	dmadone(int);
void	dmaend(int);
int	dmainit(int, int);
long	dmasetup(int, void*, long, int);
#define	evenaddr(x)				/* x86 doesn't care */
void	fpclear(void);
void	fpenv(FPsave*);
void	fpinit(void);
void	fpoff(void);
void	fprestore(FPsave*);
void	fpsave(FPsave*);
ulong	fpstatus(void);
ulong	getcr0(void);
ulong	getcr2(void);
ulong	getcr3(void);
ulong	getcr4(void);
char*	getconf(char*);
void	guesscpuhz(int);
void	halt(void);
int	i8042auxcmd(int);
int	i8042auxcmds(uchar*, int);
void	i8042auxenable(void (*)(int, int));
void	i8042reset(void);
void	i8250console(void);
void*	i8250alloc(int, int, int);
void	i8250mouse(char*, int (*)(Queue*, int), int);
void	i8250setmouseputc(char*, int (*)(Queue*, int));
void	i8253enable(void);
void	i8253init(void);
void	i8253link(void);
uvlong	i8253read(uvlong*);
void	i8253timerset(uvlong);
int	i8259disable(int);
int	i8259enable(Vctl*);
void	i8259init(void);
int	i8259isr(int);
void	i8259on(void);
void	i8259off(void);
int	i8259vecno(int);
void	idle(void);
void	idlehands(void);
int	inb(int);
void	insb(int, void*, int);
ushort	ins(int);
void	inss(int, void*, int);
ulong	inl(int);
void	insl(int, void*, int);
int	intrdisable(int, void (*)(Ureg *, void *), void*, int, char*);
void	intrenable(int, void (*)(Ureg*, void*), void*, int, char*);
void	introff(void);
void	intron(void);
void	invlpg(ulong);
void	iofree(int);
void	ioinit(void);
int	iounused(int, int);
int	ioalloc(int, int, int, char*);
int	ioreserve(int, int, int, char*);
int	iprint(char*, ...);
int	isaconfig(char*, int, ISAConf*);
void*	kaddr(ulong);
void	kbdenable(void);
void	kbdinit(void);
#define	kmapinval()
void	lgdt(ushort[3]);
void	lidt(ushort[3]);
void	links(void);
void	ltr(ulong);
void	mach0init(void);
void	mathinit(void);
void	mb386(void);
void	mb586(void);
void	meminit(void);
void	memorysummary(void);
#define mmuflushtlb(pdb) putcr3(pdb)
void	mmuinit(void);
ulong*	mmuwalk(ulong*, ulong, int, int);
int	mtrr(uvlong, uvlong, char *);
void	mtrrclock(void);
int	mtrrprint(char *, long);
uchar	nvramread(int);
void	nvramwrite(int, uchar);
void	outb(int, int);
void	outsb(int, void*, int);
void	outs(int, ushort);
void	outss(int, void*, int);
void	outl(int, ulong);
void	outsl(int, void*, int);
ulong	paddr(void*);
void	pcireset(void);
void	pcmcisread(PCMslot*);
int	pcmcistuple(int, int, int, void*, int);
PCMmap*	pcmmap(int, ulong, int, int);
int	pcmspecial(char*, ISAConf*);
int	(*_pcmspecial)(char *, ISAConf *);
void	pcmspecialclose(int);
void	(*_pcmspecialclose)(int);
void	pcmunmap(int, PCMmap*);
int	pdbmap(ulong*, ulong, ulong, int);
void	procrestore(Proc*);
void	procsave(Proc*);
void	procsetup(Proc*);
void	putcr0(ulong);
void	putcr3(ulong);
void	putcr4(ulong);
void*	rampage(void);
void	rdmsr(int, vlong*);
void	realmode(Ureg*);
void	screeninit(void);
void	(*screenputs)(char*, int);
void	syncclock(void);
void*	tmpmap(Page*);
void	tmpunmap(void*);
void	touser(void*);
void	trapenable(int, void (*)(Ureg*, void*), void*, char*);
void	trapinit(void);
void	trapinit0(void);
int	tas(void*);
uvlong	tscticks(uvlong*);
ulong	umbmalloc(ulong, int, int);
void	umbfree(ulong, int);
ulong	umbrwmalloc(ulong, int, int);
void	umbrwfree(ulong, int);
ulong	upaalloc(int, int);
void	upafree(ulong, int);
void	upareserve(ulong, int);
#define	userureg(ur) (((ur)->cs & 0xFFFF) == UESEL)
void	vectortable(void);
void*	vmap(ulong, int);
int	vmapsync(ulong);
void	vunmap(void*, int);
void	wbinvd(void);
void	wrmsr(int, vlong);
int	xchgw(ushort*, int);

#define	waserror()	(up->nerrlab++, setlabel(&up->errlab[up->nerrlab-1]))
#define	KADDR(a)	kaddr(a)
#define PADDR(a)	paddr((void*)(a))

#define	dcflush(a, b)

// Plan 9 VX additions
void	gotolabel(Label*);
int	isuaddr(void*);
void	labelinit(Label *l, ulong pc, ulong sp);
void	latin1putc(int, void(*)(int));
void	makekprocdev(Dev*);
void	newmach(void);
void	oserror(void);
void	oserrstr(void);
void restoretty(void);
int	setlabel(Label*);
void	setsigsegv(int invx32);
int	tailkmesg(char*, int);
void	trap(Ureg*);
void	uartecho(char*, int);
void	uartinit(int);
void	*uvalidaddr(ulong addr, ulong len, int write);

#define GSHORT(p)	(((p)[1]<<8)|(p)[0])
#define GLONG(p)	((GSHORT(p+2)<<16)|GSHORT(p))

void	__plock(Psleep*);
void	__punlock(Psleep*);
void	__pwakeup(Psleep*);
void	__psleep(Psleep*);

extern int tracelock;

#define lockfngen(type)	__ ## type

#define lockgen(type, arg) 								\
	do {										\
		if (tracelock) {							\
			iprint("%s %p %s %d\n", (#type), (arg), __FILE__, __LINE__);	\
			lockfngen(type)((arg));						\
		} else {								\
			lockfngen(type)((arg));						\
		}									\
	} while (0)

#define qlock(x)	lockgen(qlock, (x))
#define qunlock(x)	lockgen(qunlock, (x))
#define rlock(x)	lockgen(rlock, (x))
#define runlock(x)	lockgen(runlock, (x))
#define wlock(x)	lockgen(wlock, (x))
#define wunlock(x)	lockgen(wunlock, (x))
#define plock(x)	lockgen(plock, (x))
#define punlock(x)	lockgen(punlock, (x))
#define pwakeup(x)	lockgen(pwakeup, (x))
#define psleep(x)	lockgen(psleep, (x))
// #define lock(x)		lockgen(lock, (x))
// #define unlock(x)	lockgen(unlock, (x))
#define lock(x) __lock(x)
#define unlock(x) __unlock(x)
#define canqlock	__canqlock
#define canrlock	__canrlock

#define	LOCK(x)		lock(&((x)->lk))
#define	UNLOCK(x)	unlock(&((x)->lk))
#define CANQLOCK(x)	canqlock(&((x)->qlock))
#define	QLOCK(x)	qlock(&((x)->qlock))
#define	QUNLOCK(x)	qunlock(&((x)->qlock))
#define CANRLOCK(x)	canrlock(&((x)->rwlock))
#define	RLOCK(x)	rlock(&((x)->rwlock))
#define	RUNLOCK(x)	runlock(&((x)->rwlock))
#define	WLOCK(x)	wlock(&((x)->rwlock))
#define	WUNLOCK(x)	wunlock(&((x)->rwlock))

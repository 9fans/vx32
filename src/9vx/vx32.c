/*
 * User-space execution.
 *
 * On a real x86 processor, Plan 9 calls touser to execute an IRET
 * instruction to go back into user space, and then eventually a trap
 * happens and the processor is magically transported back into
 * kernel space running trap or syscall.  
 *
 * In Plan 9 VX, touser calls into vx32 to manage user execution;
 * vx32 eventually returns a trap code and then touser dispatches
 * to trap or syscall as appropriate.  When trap or syscall returns,
 * touser calls back into vx32 and the cycle repeats.
 */

#define	WANT_M

#include "u.h"
#include <pthread.h>
#include <sys/mman.h>
#include "libvx32/vx32.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "error.h"
#include "ureg.h"

enum {
	ClockTicks = 1,
	ClockMillis = 25,
};

int nfaults;
int traceprocs;
int tracesyscalls;
extern char *sysctab[];
extern void mathemu(Ureg*, void*);

static void proc2ureg(vxproc*, Ureg*);
static void ureg2proc(Ureg*, vxproc*);

static vxmem thevxmem;

void
vx32sysr1(void)
{
//	traceprocs = !traceprocs;
//	vx32_debugxlate = traceprocs;
	tracesyscalls = !tracesyscalls;
}

/*
 * Vxnewproc is called at the end of newproc
 * to fill in vx32-specific entries in the Proc struct
 * before it gets used.
 */
void
vxnewproc(Proc *p)
{
	PMMU *pm;
	
	pm = &p->pmmu;

	/*
	 * Kernel procs don't need vxprocs; if this proc
	 * already has one, take it away.  Also, give
	 * kernel procs very large stacks so they can call
	 * into non-thread-friendly routines like x11 
	 * and getgrgid.
	 */
	if(p->kp){
		if(pm->vxproc){
			pm->vxproc->mem = nil;
			vxproc_free(pm->vxproc);
			pm->vxproc = nil;
		}
		free(p->kstack);
		p->kstack = nil;
		p->kstack = smalloc(512*1024);
		return;
	}

	pm->lo = 0x80000000UL;
	pm->hi = 0;
	if(pm->vxproc == nil){
		pm->vxproc = vxproc_alloc();
		if(pm->vxproc == nil)
			panic("vxproc_alloc");
		pm->vxproc->mem = &thevxmem;
	}
}

/*
 * Vx32 hooks to read, write, map, unmap, and check permissions
 * on user memory.  Normally these are more involved, but we're
 * using the processor to do everything.
 */
static ssize_t
vmread(vxmem *vm, void *data, uint32_t addr, uint32_t len)
{
	memmove(data, uzero+addr, len);
	return len;
}

static ssize_t
vmwrite(vxmem *vm, const void *data, uint32_t addr, uint32_t len)
{
	memmove(uzero+addr, data, len);
	return len;
}

static vxmmap thevxmmap =
{
	1,
	(void*)-1,	/* to be filled in with user0 */
	USTKTOP,
};

static vxmmap*
vmmap(vxmem *vm, uint32_t flags)
{
	thevxmmap.base = uzero;
	return &thevxmmap;
}

static void
vmunmap(vxmem *vm, vxmmap *mm)
{
}

static int
vmcheckperm(vxmem *vm, uint32_t addr, uint32_t len, uint32_t perm, uint32_t *out_faultva)
{
	/* All is allowed - handle faults as they happen. */
	return 1;
}

static int
vmsetperm(vxmem *vm, uint32_t addr, uint32_t len, uint32_t perm)
{
	return 0;
}

static int
vmresize(vxmem *vm, size_t size)
{
	return 0;
}

static void
vmfree(vxmem *vm)
{
}

static vxmem thevxmem =
{
	vmread,
	vmwrite,
	vmmap,
	vmunmap,
	vmcheckperm,
	vmsetperm,
	vmresize,
	vmfree,
};

static void
setclock(int start)
{
	struct itimerval itv;

	/* Ask for clock tick to interrupt execution after ClockMillis ms. */
	memset(&itv, 0, sizeof itv);
	if(start)
		itv.it_value.tv_usec = ClockMillis*1000;
	else
		itv.it_value.tv_usec = 0;
	setitimer(ITIMER_VIRTUAL, &itv, 0);
}

/*
 * Newly forked processes start executing at forkret.
 * The very first process, init, starts executing at touser(sp),
 * where sp is its stack pointer.
 */
void
forkret(void)
{
	touser(0);
}

void
touser(void *initsp)
{
	int rc;
	void *kp;
	vxproc *vp;
	Ureg u;
	Ureg *u1;
	uchar *addr;

	vp = up->pmmu.vxproc;
	if(initsp){
		/* init: clear register set, setup sp, eip */
		memset(vp->cpu, 0, sizeof *vp->cpu);
		vp->cpu->reg[ESP] = (ulong)initsp;
		vp->cpu->eflags = 0;
		vp->cpu->eip = UTZERO+32;
	}else{
		/* anyone else: registers are sitting at top of kernel stack */
		kp = (char*)up->kstack + KSTACK - (sizeof(Ureg) + 2*BY2WD);
		u1 = (Ureg*)((char*)kp + 2*BY2WD);
		ureg2proc(u1, vp);
	}

	/*
	 * User-mode execution loop.
	 */
	for(;;){
		/*
		 * Optimization: try to fault in code page and stack
		 * page right now, since we're likely to need them.
		 */
		if(up->pmmu.hi == 0){
			fault(vp->cpu->eip, 1);
			fault(vp->cpu->reg[ESP], 0);
		}
		
		/*
		 * Let vx32 know whether to allow floating point.
		 * TODO: Fix vx32 so that you don't need to flush
		 * on the transition from FPinactive -> FPactive.
		 */
		if(vp->allowfp != (up->fpstate == FPactive)){
			vp->allowfp = (up->fpstate == FPactive);
			vxproc_flush(vp);
		}

		if(traceprocs)
			iprint("+vx32 %p %p %s eip=%lux esp=%lux\n",
				m, up, up->text, vp->cpu->eip, vp->cpu->reg[ESP]);

		setsigsegv(1);
		setclock(1);
		rc = vxproc_run(vp);
		setclock(0);
		setsigsegv(0);

		if(rc < 0)
			panic("vxproc_run: %r");

		if(traceprocs)
			iprint("-vx32 %p %p %s eip=%lux esp=%lux rc=%#x\n",
				m, up, up->text, vp->cpu->eip, vp->cpu->reg[ESP], rc);

		/*
		 * Handle page faults quickly, without proc2ureg, ureg2proc,
		 * if possible.  Otherwise fall back to default trap call.
		 */
		if(rc == VXTRAP_PAGEFAULT){
			int read;
			nfaults++;
			read = !(vp->cpu->traperr & 2);
			addr = (uchar*)vp->cpu->trapva;
			if(traceprocs)
				print("fault %p read=%d\n", addr, read);
			if(isuaddr(addr) && fault(addr - uzero, read) >= 0)
				continue;
			print("%ld %s: unhandled fault va=%lux [%lux] eip=%lux\n",
				up->pid, up->text,
				addr - uzero, vp->cpu->trapva, vp->cpu->eip);
			proc2ureg(vp, &u);
			dumpregs(&u);
			if(doabort)
				abort();
		}

		up->dbgreg = &u;
		proc2ureg(vp, &u);
		u.trap = rc;
		trap(&u);
		ureg2proc(&u, vp);
	}
}

static void
proc2ureg(vxproc *vp, Ureg *u)
{
	memset(u, 0, sizeof *u);
	u->pc = vp->cpu->eip;
	u->ax = vp->cpu->reg[EAX];
	u->bx = vp->cpu->reg[EBX];
	u->cx = vp->cpu->reg[ECX];
	u->dx = vp->cpu->reg[EDX];
	u->si = vp->cpu->reg[ESI];
	u->di = vp->cpu->reg[EDI];
	u->usp = vp->cpu->reg[ESP];
}

static void
ureg2proc(Ureg *u, vxproc *vp)
{
	vp->cpu->eip = u->pc;
	vp->cpu->reg[EAX] = u->ax;
	vp->cpu->reg[EBX] = u->bx;
	vp->cpu->reg[ECX] = u->cx;
	vp->cpu->reg[EDX] = u->dx;
	vp->cpu->reg[ESI] = u->si;
	vp->cpu->reg[EDI] = u->di;
	vp->cpu->reg[ESP] = u->usp;
}


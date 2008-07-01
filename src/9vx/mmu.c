#define	WANT_M
#include "u.h"
#include <pthread.h>
#include "libvx32/vx32.h"
#include <sys/mman.h>
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "error.h"
#include "ureg.h"

int tracemmu;

#ifndef MAP_ANONYMOUS 
#define MAP_ANONYMOUS MAP_ANON
#endif
#define MAP_EMPTY (MAP_FIXED|MAP_ANONYMOUS|MAP_PRIVATE)

/*
 * We allocate a 256MB page file on disk to hold the "physical memory".
 * We'll mmap individual file pages where we need them to simulate
 * the page translation of a real MMU.  To make the simulation more
 * faithful, we map the vx32 sandboxed address space starting at 0,
 * so that kernel 0 = user 0, so that pointers can be shared.
 * Plan 9 assumes this, and while it's not a ton of work to break that
 * assumption, it was easier not to.
 */
#define MEMSIZE (256<<20)

static int pagefile;
static char* pagebase;
uchar *uzero;

int
isuaddr(void *v)
{
	uchar *p;
	
	p = v;
	return uzero <= p && p < uzero+USTKTOP;
}

/*
 * Allocate a process-sized mapping with nothing there.
 * The point is to reserve the space so that
 * nothing else ends up there later.
 */
static void
mapzero(void)
{
	int fd;
	void *v;
	
	/* First try mmaping /dev/zero.  Some OS'es don't allow this. */
	if((fd = open("/dev/zero", O_RDONLY)) >= 0){
		v = mmap(nil, USTKTOP, PROT_NONE, MAP_PRIVATE, fd, 0);
		if(v != MAP_FAILED){
			uzero = v;
			return;
		}
	}
	
	/* Next try an anonymous map. */
	v = mmap(nil, USTKTOP, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if(v != MAP_FAILED){
		uzero = v;
		return;
	}
	
	panic("mapzero: cannot reserve process address space");
}

void
mmuinit(void)
{
	char tmp[] = "/var/tmp/9vx.pages.XXXXXX";
	void *v;

	mapzero();
	if((pagefile = mkstemp(tmp)) < 0)
		panic("mkstemp: %r");
	if(ftruncate(pagefile, MEMSIZE) < 0)
		panic("ftruncate pagefile: %r");
	unlink(tmp);	/* "remove on close" */

	/* Map pages for direct access at pagebase, wherever that is */
	/* MAP_SHARED means write the changes back to the file */
	v = mmap(nil, MEMSIZE, PROT_READ|PROT_WRITE,
		MAP_SHARED, pagefile, 0);
	if(v == MAP_FAILED)	
		panic("mmap pagefile: %r");
	pagebase = v;

	conf.mem[0].base = 0;
	conf.mem[0].npage = MEMSIZE / BY2PG;
	
	palloc.mem[0].base = 0;
	palloc.mem[0].npage = MEMSIZE / BY2PG;
}

/*
 * Temporary page mappings are easy again:
 * everything is mapped at PAGEBASE.
 */
void*
tmpmap(Page *pg)
{
	assert(pg->pa < MEMSIZE);
	return pagebase + pg->pa;
}

void
tmpunmap(void *v)
{
	assert(pagebase <= (char*)v && (char*)v < pagebase + MEMSIZE);
}

KMap*
kmap(Page *p)
{
	return (KMap*)tmpmap(p);
}

void
kunmap(KMap *k)
{
}

/*
 * The process whose address space we've got mapped.
 * We cache our own copy so that entering the scheduler
 * and coming back out running the same process doesn't
 * cause unnecessary unmapping and remapping.
 */
static Proc *mmup;

/*
 * Flush the current address space.
 */
static void
mmapflush(void)
{
	m->flushmmu = 0;

	/* Nothing mapped? */
	if(mmup == nil || mmup->pmmu.lo > mmup->pmmu.hi)
		return;

#ifdef __FreeBSD__
	if(__FreeBSD__ < 7){
		/*
		 * On FreeBSD, we need to be able to use mincore to
		 * tell whether a page is mapped, so we have to remap
		 * something with no pages here. 
		 */
		if(mmap(uzero, mmup->pmmu.hi+BY2PG, PROT_NONE, 
				MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
			panic("mmapflush mmap: %r");
		mmup->pmmu.lo = 0x80000000UL;
		mmup->pmmu.hi = 0;
		return;
	}
#endif

	/* Clear only as much as got mapped. */
	if(mprotect(uzero, mmup->pmmu.hi+BY2PG, PROT_NONE) < 0)
		panic("mmapflush mprotect: %r");
	mmup->pmmu.lo = 0x80000000UL;
	mmup->pmmu.hi = 0;
}

/*
 * Update the "MMU" in response to a user fault. 
 * pa may have PTEWRITE set.
 */
void
putmmu(ulong va, ulong pa, Page *p)
{
	int prot;
	PMMU *pmmu;

	if(tracemmu || (pa&~(PTEWRITE|PTEVALID)) != p->pa)
		print("putmmu va %lux pa %lux p->pa %lux\n", va, pa, p->pa);

	assert(p->pa < MEMSIZE && pa < MEMSIZE);
	assert(up);

	/* Map the page */
	prot = PROT_READ;
	if(pa&PTEWRITE)
		prot |= PROT_WRITE;
	pa &= ~(BY2PG-1);
	va  &= ~(BY2PG-1);
	if(mmap(uzero+va, BY2PG, prot, MAP_FIXED|MAP_SHARED,
			pagefile, pa) == MAP_FAILED)
		panic("putmmu");
	
	/* Record high and low address range for quick unmap. */
	pmmu = &up->pmmu;
	if(pmmu->lo > va)
		pmmu->lo = va;
	if(pmmu->hi < va)
		pmmu->hi = va;
//	printlinuxmaps();
}

/*
 * The memory maps have changed.  Flush all cached state.
 */
void
flushmmu(void)
{
	if(tracemmu)
		print("flushmmu\n");

	if(up)
		vxproc_flush(up->pmmu.vxproc);
	mmapflush();
}

/*
 * Called when scheduler has decided to run proc p.
 * Prepare to run proc p.
 */
void
mmuswitch(Proc *p)
{
	/*
	 * Switch the address space, but only if it's not the
	 * one we were just in.  Also, kprocs don't count --
	 * only the guys on cpu0 do.
	 */
	if(!p->kp && (mmup != p || p->newtlb || m->flushmmu)){
		if(0) print("^^^^^^^^^^ %ld %s\n========== %ld %s\n",
			mmup ? mmup->pid : 0, mmup? mmup->text : "",
			p->pid, p->text);
		/* No vxproc_flush - vxproc cache is okay */
		mmapflush();
		p->newtlb = 0;
		mmup = p;
	}
}

/*
 * Called when proc p is dying.
 */
void
mmurelease(Proc *p)
{
	if(p->kp)
		return;
	if(p->pmmu.vxproc)
		vxproc_flush(p->pmmu.vxproc);
	if(p == mmup || m->flushmmu){
		mmapflush();
		mmup = nil;
	}
}

void
printlinuxmaps(void)
{
	char buf[100];
	sprint(buf, "cat /proc/%d/maps", getpid());
	system(buf);
}

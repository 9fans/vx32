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

static Uspace uspace[16];
static Uspace *ulist[nelem(uspace)];
int nuspace = 1;

int
isuaddr(void *v)
{
	uchar *p;
	uchar *uzero;
	
	p = v;
	uzero = up->pmmu.uzero;
	return uzero <= p && p < uzero+USTKTOP;
}

/*
 * Allocate a process-sized mapping with nothing there.
 * The point is to reserve the space so that
 * nothing else ends up there later.
 */
static void*
mapzero(void)
{
	int fd, bit32;
	void *v;
	
#ifdef i386
	bit32 = 0;
#else
	bit32 = MAP_32BIT;
#endif
	/* First try mmaping /dev/zero.  Some OS'es don't allow this. */
	if((fd = open("/dev/zero", O_RDONLY)) >= 0){
		v = mmap(nil, USTKTOP, PROT_NONE, bit32|MAP_PRIVATE, fd, 0);
		if(v != MAP_FAILED) {
			if((uint32_t)(uintptr)v != (uintptr)v) {
				iprint("mmap returned 64-bit pointer %p\n", v);
				panic("mmap");
			}
			return v;
		}
	}
	
	/* Next try an anonymous map. */
	v = mmap(nil, USTKTOP, PROT_NONE, bit32|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if(v != MAP_FAILED) {
		if((uint32_t)(uintptr)v != (uintptr)v) {
			iprint("mmap returned 64-bit pointer %p\n", v);
			panic("mmap");
		}
		return v;
	}

	return nil;
}

void
mmuinit(void)
{
	char tmp[] = "/var/tmp/9vx.pages.XXXXXX";
	void *v;
	int i;
	
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

	if(nuspace <= 0)
		nuspace = 1;
	if(nuspace > nelem(uspace))
		nuspace = nelem(uspace);
	for(i=0; i<nuspace; i++){
		uspace[i].uzero = mapzero();
		if(uspace[i].uzero == nil)
			panic("mmap address space %d", i);
		ulist[i] = &uspace[i];
	}

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
 * Flush the current address space.
 */
static void
mmapflush(Uspace *us)
{
	m->flushmmu = 0;

	/* Nothing mapped? */
	if(us == nil || us->lo > us->hi || us->uzero == nil)
		return;

#ifdef __FreeBSD__
	if(__FreeBSD__ < 7){
		/*
		 * On FreeBSD, we need to be able to use mincore to
		 * tell whether a page is mapped, so we have to remap
		 * something with no pages here. 
		 */
		if(mmap(us->uzero, us->hi+BY2PG, PROT_NONE, 
				MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
			panic("mmapflush mmap: %r");
		us->lo = 0x80000000UL;
		us->hi = 0;
		return;
	}
#endif

	/* Clear only as much as got mapped. */
	if(mprotect(us->uzero, us->hi+BY2PG, PROT_NONE) < 0)
		panic("mmapflush mprotect: %r");
	us->lo = 0x80000000UL;
	us->hi = 0;
}

/*
 * Update the "MMU" in response to a user fault. 
 * pa may have PTEWRITE set.
 */
void
putmmu(ulong va, ulong pa, Page *p)
{
	int prot;
	Uspace *us;

	if(tracemmu || (pa&~(PTEWRITE|PTEVALID)) != p->pa)
		print("putmmu va %lux pa %lux p->pa %lux\n", va, pa, p->pa);

	assert(p->pa < MEMSIZE && pa < MEMSIZE);
	assert(up);
	us = up->pmmu.us;
	assert(us);

	/* Map the page */
	prot = PROT_READ;
	if(pa&PTEWRITE)
		prot |= PROT_WRITE;
	pa &= ~(BY2PG-1);
	va  &= ~(BY2PG-1);
	if(mmap(us->uzero+va, BY2PG, prot, MAP_FIXED|MAP_SHARED,
			pagefile, pa) == MAP_FAILED)
		panic("putmmu");
	
	/* Record high and low address range for quick unmap. */
	if(us->lo > va)
		us->lo = va;
	if(us->hi < va)
		us->hi = va;
//	printlinuxmaps();
}

/*
 * The memory maps have changed for up.  Flush all cached state.
 */
void
flushmmu(void)
{
	if(tracemmu)
		print("flushmmu\n");

	if(up){
		vxproc_flush(up->pmmu.vxproc);
		mmapflush(up->pmmu.us);
	}
}

void
usespace(Uspace *us)
{
	int i;
	
	for(i=0; i<nuspace; i++)
		if(ulist[i] == us){
			while(i > 0){
				ulist[i] = ulist[i-1];
				i--;
			}
			ulist[0] = us;
			break;
		}
}

Uspace*
getspace(Proc *p)
{
	Uspace *us;
	
	us = ulist[nuspace-1];
	if(us->p){
		if(tracemmu)
			print("^^^^^^^^^^ %ld %s [evict %d]\n", us->p->pid, us->p->text, us - uspace);
		mmapflush(us);
	}
	us->p = p;
	p->pmmu.vxmm.base = us->uzero;
	p->pmmu.uzero = us->uzero;
	p->pmmu.us = us;
	usespace(us);
	return us;
}

void
takespace(Proc *p, Uspace *us)
{
	usespace(us);
	if(us->p == p)
		return;
	if(tracemmu){
		if(us->p)
			print("^^^^^^^^^^ %ld %s [steal %d]\n", us->p->pid, us->p->text, us - uspace);
	}
	us->p = p;
	mmapflush(us);
}

void
putspace(Uspace *us)
{
	int i;

	mmapflush(us);
	us->p->pmmu.us = nil;
	us->p->pmmu.uzero = nil;
	us->p->pmmu.vxmm.base = nil;
	us->p = nil;
	for(i=0; i<nuspace; i++)
		if(ulist[i] == us){
			while(++i < nuspace)
				ulist[i-1] = ulist[i];
			ulist[i-1] = us;
			break;
		}
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
	if(p->kp)
		return;
	
	if(tracemmu)
		print("mmuswitch %ld %s\n", p->pid, p->text);

	if(p->pmmu.us && p->pmmu.us->p == p){
		if(tracemmu) print("---------- %ld %s [%d]\n",
			p->pid, p->text, p->pmmu.us - uspace);
		usespace(p->pmmu.us);
		if(!p->newtlb && !m->flushmmu){
			usespace(p->pmmu.us);
			return;
		}
		mmapflush(p->pmmu.us);
		p->newtlb = 0;
		return;
	}

	if(p->pmmu.us == nil)
		getspace(p);
	else
		takespace(p, p->pmmu.us);
	if(tracemmu) print("========== %ld %s [%d]\n",
		p->pid, p->text, p->pmmu.us - uspace);
}

/*
 * Called when proc p is dying.
 */
void
mmurelease(Proc *p)
{
	if(p->kp)
		return;
	if(tracemmu)
		print("mmurelease %ld %s\n", p->pid, p->text);
	if(p->pmmu.vxproc)
		vxproc_flush(p->pmmu.vxproc);
	if(p->pmmu.us){
		if(tracemmu)
			print("^^^^^^^^^^ %ld %s [release %d]\n", p->pid, p->text, p->pmmu.us - uspace);
		putspace(p->pmmu.us);
		if(m->flushmmu)
			mmapflush(p->pmmu.us);
	}
}

void
printlinuxmaps(void)
{
	char buf[100];
	sprint(buf, "cat /proc/%d/maps", getpid());
	system(buf);
}

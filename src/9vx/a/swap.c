#include	"u.h"
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"error.h"

static int	canflush(Proc*, Segment*);
static void	executeio(void);
static int	needpages(void *v);
static void	pageout(Proc*, Segment*);
static void	pagepte(int, Page**);
static void	pager(void *v);

	Image 	swapimage;
static	Page	**iolist;
static	int	ioptr;

void
swapinit(void)
{
	swapalloc.swmap = xalloc(conf.nswap);
	swapalloc.top = &swapalloc.swmap[conf.nswap];
	swapalloc.alloc = swapalloc.swmap;
	swapalloc.last = swapalloc.swmap;
	swapalloc.free = conf.nswap;
	iolist = xalloc(conf.nswppo*sizeof(Page*));
	if(swapalloc.swmap == 0 || iolist == 0)
		panic("swapinit: not enough memory");

	swapimage.notext = 1;
}

ulong
newswap(void)
{
	uchar *look;

	lock(&swapalloc.lk);

	if(swapalloc.free == 0){
		unlock(&swapalloc.lk);
		return ~0;
	}

	look = memchr(swapalloc.last, 0, swapalloc.top-swapalloc.last);
	if(look == 0)
		panic("inconsistent swap");

	*look = 1;
	swapalloc.last = look;
	swapalloc.free--;
	unlock(&swapalloc.lk);
	return (look-swapalloc.swmap) * BY2PG;
}

void
putswap(Page *p)
{
	uchar *idx;

	lock(&swapalloc.lk);
	idx = &swapalloc.swmap[((ulong)p)/BY2PG];
	if(--(*idx) == 0) {
		swapalloc.free++;
		if(idx < swapalloc.last)
			swapalloc.last = idx;
	}
	if(*idx >= 254)
		panic("putswap %lux == %ud", p, *idx);
	unlock(&swapalloc.lk);
}

void
dupswap(Page *p)
{
	lock(&swapalloc.lk);
	if(++swapalloc.swmap[((ulong)p)/BY2PG] == 0)
		panic("dupswap");
	unlock(&swapalloc.lk);
}

int
swapcount(ulong daddr)
{
	return swapalloc.swmap[daddr/BY2PG];
}

void
kickpager(void)
{
	static int started;

	if(started)
		wakeup(&swapalloc.r);
	else {
		kproc("pager", pager, 0);
		started = 1;
	}
}

static void
pager(void *junk)
{
	int i;
	Segment *s;
	Proc *p, *ep;

	if(waserror())
		panic("pager: os error\n");

	p = proctab(0);
	ep = &p[conf.nproc];

loop:
	up->psstate = "Idle";
	sleep(&swapalloc.r, needpages, 0);
print("uh oh.  someone woke the pager\n");

	while(needpages(junk)) {

		if(swapimage.c) {
			p++;
			if(p >= ep)
				p = proctab(0);
	
			if(p->state == Dead || p->noswap)
				continue;

			if(!canqlock(&p->seglock))
				continue;		/* process changing its segments */

			for(i = 0; i < NSEG; i++) {
				if(!needpages(junk)){
					qunlock(&p->seglock);
					goto loop;
				}

				if((s = p->seg[i])) {
					switch(s->type&SG_TYPE) {
					default:
						break;
					case SG_TEXT:
						pageout(p, s);
						break;
					case SG_DATA:
					case SG_BSS:
					case SG_STACK:
					case SG_SHARED:
						up->psstate = "Pageout";
						pageout(p, s);
						if(ioptr != 0) {
							up->psstate = "I/O";
							executeio();
						}
						break;
					}
				}
			}
			qunlock(&p->seglock);
		}
		else {
			print("out of physical memory; no swap configured\n");
			if(!cpuserver || freebroken() == 0)
				killbig("out of memory");

			/* Emulate the old system if no swap channel */
			tsleep(&up->sleep, return0, 0, 5000);
			wakeup(&palloc.r);
		}
	}
	goto loop;
}

static void
pageout(Proc *p, Segment *s)
{
	int type, i, size;
	Pte *l;
	Page **pg, *entry;

	if(!canqlock(&s->lk))	/* We cannot afford to wait, we will surely deadlock */
		return;

	if(s->steal) {		/* Protected by /dev/proc */
		qunlock(&s->lk);
		return;
	}

	if(!canflush(p, s)) {	/* Able to invalidate all tlbs with references */
		qunlock(&s->lk);
		putseg(s);
		return;
	}

	if(waserror()) {
		qunlock(&s->lk);
		putseg(s);
		return;
	}

	/* Pass through the pte tables looking for memory pages to swap out */
	type = s->type&SG_TYPE;
	size = s->mapsize;
	for(i = 0; i < size; i++) {
		l = s->map[i];
		if(l == 0)
			continue;
		for(pg = l->first; pg < l->last; pg++) {
			entry = *pg;
			if(pagedout(entry))
				continue;

			if(entry->modref & PG_REF) {
				entry->modref &= ~PG_REF;
				continue;
			}

			pagepte(type, pg);

			if(ioptr >= conf.nswppo)
				goto out;
		}
	}
out:
	poperror();
	qunlock(&s->lk);
	putseg(s);
}

static int
canflush(Proc *p, Segment *s)
{
	int i;
	Proc *ep;

	lock(&s->ref.lk);
	if(s->ref.ref == 1) {		/* Easy if we are the only user */
		s->ref.ref++;
		unlock(&s->ref.lk);
		return canpage(p);
	}
	s->ref.ref++;
	unlock(&s->ref.lk);

	/* Now we must do hardwork to ensure all processes which have tlb
	 * entries for this segment will be flushed if we succeed in paging it out
	 */
	p = proctab(0);
	ep = &p[conf.nproc];
	while(p < ep) {
		if(p->state != Dead) {
			for(i = 0; i < NSEG; i++)
				if(p->seg[i] == s)
					if(!canpage(p))
						return 0;
		}
		p++;
	}
	return 1;
}

static void
pagepte(int type, Page **pg)
{
	ulong daddr;
	Page *outp;

	outp = *pg;
	switch(type) {
	case SG_TEXT:				/* Revert to demand load */
		putpage(outp);
		*pg = 0;
		break;

	case SG_DATA:
	case SG_BSS:
	case SG_STACK:
	case SG_SHARED:
		/*
		 *  get a new swap address and clear any pages
		 *  referring to it from the cache
		 */
		daddr = newswap();
		if(daddr == ~0)
			break;
		cachedel(&swapimage, daddr);

		lock(&outp->lk);

		/* forget anything that it used to cache */
		uncachepage(outp);

		/*
		 *  incr the reference count to make sure it sticks around while
		 *  being written
		 */
		outp->ref++;

		/*
		 *  enter it into the cache so that a fault happening
		 *  during the write will grab the page from the cache
		 *  rather than one partially written to the disk
		 */
		outp->daddr = daddr;
		cachepage(outp, &swapimage);
		*pg = (Page*)(daddr|PG_ONSWAP);
		unlock(&outp->lk);

		/* Add page to IO transaction list */
		iolist[ioptr++] = outp;
		break;
	}
}

void
pagersummary(void)
{
	print("%lud/%lud memory %lud/%lud swap %d iolist\n",
		palloc.user-palloc.freecount,
		palloc.user, conf.nswap-swapalloc.free, conf.nswap,
		ioptr);
}

static void
executeio(void)
{
	Page *out;
	int i, n;
	Chan *c;
	char *kaddr;
	KMap *k;

	c = swapimage.c;

	for(i = 0; i < ioptr; i++) {
		if(ioptr > conf.nswppo)
			panic("executeio: ioptr %d > %d\n", ioptr, conf.nswppo);
		out = iolist[i];
		k = kmap(out);
		kaddr = (char*)VA(k);

		if(waserror())
			panic("executeio: page out I/O error");

		n = devtab[c->type]->write(c, kaddr, BY2PG, out->daddr);
		if(n != BY2PG)
			nexterror();

		kunmap(k);
		poperror();

		/* Free up the page after I/O */
		lock(&out->lk);
		out->ref--;
		unlock(&out->lk);
		putpage(out);
	}
	ioptr = 0;
}

static int
needpages(void *v)
{
	return palloc.freecount < swapalloc.headroom;
}

void
setswapchan(Chan *c)
{
	uchar dirbuf[sizeof(Dir)+100];
	Dir d;
	int n;

	if(swapimage.c) {
		if(swapalloc.free != conf.nswap){
			cclose(c);
			error(Einuse);
		}
		cclose(swapimage.c);
	}

	/*
	 *  if this isn't a file, set the swap space
	 *  to be at most the size of the partition
	 */
	if(devtab[c->type]->dc != L'M'){
		n = devtab[c->type]->stat(c, dirbuf, sizeof dirbuf);
		if(n <= 0){
			cclose(c);
			error("stat failed in setswapchan");
		}
		convM2D(dirbuf, n, &d, nil);
		if(d.length < conf.nswap*BY2PG){
			conf.nswap = d.length/BY2PG;
			swapalloc.top = &swapalloc.swmap[conf.nswap];
			swapalloc.free = conf.nswap;
		}
	}

	swapimage.c = c;
}

int
swapfull(void)
{
	return swapalloc.free < conf.nswap/10;
}

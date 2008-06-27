#include	"u.h"
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"error.h"

#define	pghash(daddr)	palloc.hash[(daddr>>PGSHIFT)&(PGHSIZE-1)]

struct	Palloc palloc;

void
pageinit(void)
{
	int color, i, j;
	Page *p;
	Pallocmem *pm;
	ulong m, np, k, vkb, pkb;

	np = 0;
	for(i=0; i<nelem(palloc.mem); i++){
		pm = &palloc.mem[i];
		np += pm->npage;
	}
	palloc.pages = xalloc(np*sizeof(Page));
	if(palloc.pages == 0)
		panic("pageinit");

	color = 0;
	palloc.head = palloc.pages;
	p = palloc.head;
	for(i=0; i<nelem(palloc.mem); i++){
		pm = &palloc.mem[i];
		for(j=0; j<pm->npage; j++){
			p->prev = p-1;
			p->next = p+1;
			p->pa = pm->base+j*BY2PG;
			p->color = color;
			palloc.freecount++;
			color = (color+1)%NCOLOR;
			p++;
		}
	}
	palloc.tail = p - 1;
	palloc.head->prev = 0;
	palloc.tail->next = 0;

	palloc.user = p - palloc.pages;
	pkb = palloc.user*BY2PG/1024;
	vkb = pkb + (conf.nswap*BY2PG)/1024;

	/* Paging numbers */
	swapalloc.highwater = (palloc.user*5)/100;
	swapalloc.headroom = swapalloc.highwater + (swapalloc.highwater/4);

	m = 0;
	for(i=0; i<nelem(conf.mem); i++)
		if(conf.mem[i].npage)
			m += conf.mem[i].npage*BY2PG;
	k = 0;
	print("%ldM memory: ", (m+k+1024*1024-1)/(1024*1024));
	print("%ldM kernel data, ", (m+k-pkb*1024+1024*1024-1)/(1024*1024));
	print("%ldM user, ", pkb/1024);
	print("%ldM swap\n", vkb/1024);
}

static void
pageunchain(Page *p)
{
	if(canlock(&palloc.lk))
		panic("pageunchain (palloc %p)", &palloc);
	if(p->prev)
		p->prev->next = p->next;
	else
		palloc.head = p->next;
	if(p->next)
		p->next->prev = p->prev;
	else
		palloc.tail = p->prev;
	p->prev = p->next = nil;
	palloc.freecount--;
}

void
pagechaintail(Page *p)
{
	if(canlock(&palloc.lk))
		panic("pagechaintail");
	if(palloc.tail) {
		p->prev = palloc.tail;
		palloc.tail->next = p;
	}
	else {
		palloc.head = p;
		p->prev = 0;
	}
	palloc.tail = p;
	p->next = 0;
	palloc.freecount++;
}

void
pagechainhead(Page *p)
{
	if(canlock(&palloc.lk))
		panic("pagechainhead");
	if(palloc.head) {
		p->next = palloc.head;
		palloc.head->prev = p;
	}
	else {
		palloc.tail = p;
		p->next = 0;
	}
	palloc.head = p;
	p->prev = 0;
	palloc.freecount++;
}

Page*
newpage(int clear, Segment **s, ulong va)
{
	Page *p;
	KMap *k;
	uchar ct;
	int hw, dontalloc, color;

	lock(&palloc.lk);
	color = getpgcolor(va);
	hw = swapalloc.highwater;
	for(;;) {
		if(palloc.freecount > hw)
			break;
		if(up->kp && palloc.freecount > 0)
			break;

		unlock(&palloc.lk);
		dontalloc = 0;
		if(s && *s) {
			qunlock(&((*s)->lk));
			*s = 0;
			dontalloc = 1;
		}
		qlock(&palloc.pwait);	/* Hold memory requesters here */

		while(waserror())	/* Ignore interrupts */
			;

		kickpager();
		tsleep(&palloc.r, ispages, 0, 1000);

		poperror();

		qunlock(&palloc.pwait);

		/*
		 * If called from fault and we lost the segment from
		 * underneath don't waste time allocating and freeing
		 * a page. Fault will call newpage again when it has
		 * reacquired the segment locks
		 */
		if(dontalloc)
			return 0;

		lock(&palloc.lk);
	}

	/* First try for our colour */
	for(p = palloc.head; p; p = p->next)
		if(p->color == color)
			break;

	ct = PG_NOFLUSH;
	if(p == 0) {
		p = palloc.head;
		p->color = color;
		ct = PG_NEWCOL;
	}
	(void)ct;

	pageunchain(p);

	lock(&p->lk);
	if(p->ref != 0)
		panic("newpage");

	uncachepage(p);
	p->ref++;
	p->va = va;
	p->modref = 0;
	unlock(&p->lk);
	unlock(&palloc.lk);

	if(clear) {
		k = kmap(p);
		memset((void*)VA(k), 0, BY2PG);
		kunmap(k);
	}

	return p;
}

int
ispages(void *v)
{
	return palloc.freecount >= swapalloc.highwater;
}

void
putpage(Page *p)
{
	if(onswap(p)) {
		putswap(p);
		return;
	}

	lock(&palloc.lk);
	lock(&p->lk);

	if(p->ref == 0)
		panic("putpage");

	if(--p->ref > 0) {
		unlock(&p->lk);
		unlock(&palloc.lk);
		return;
	}

	if(p->image && p->image != &swapimage)
		pagechaintail(p);
	else 
		pagechainhead(p);

	if(palloc.r.p != 0)
		wakeup(&palloc.r);

	unlock(&p->lk);
	unlock(&palloc.lk);
}

Page*
auxpage(void)
{
	Page *p;

	lock(&palloc.lk);
	p = palloc.head;
	if(palloc.freecount < swapalloc.highwater) {
		unlock(&palloc.lk);
		return 0;
	}
	pageunchain(p);

	lock(&p->lk);
	if(p->ref != 0)
		panic("auxpage");
	p->ref++;
	uncachepage(p);
	unlock(&p->lk);
	unlock(&palloc.lk);

	return p;
}

static int dupretries = 15000;

int
duppage(Page *p)				/* Always call with p locked */
{
	Page *np;
	int color;
	int retries;

	retries = 0;
retry:

	if(retries++ > dupretries){
		print("duppage %d, up %p\n", retries, up);
		dupretries += 100;
		if(dupretries > 100000)
			panic("duppage\n");
		uncachepage(p);
		return 1;
	}
		

	/* don't dup pages with no image */
	if(p->ref == 0 || p->image == nil || p->image->notext)
		return 0;

	/*
	 *  normal lock ordering is to call
	 *  lock(&palloc.lk) before lock(&p->lk).
	 *  To avoid deadlock, we have to drop
	 *  our locks and try again.
	 */
	if(!canlock(&palloc.lk)){
		unlock(&p->lk);
		if(up)
			sched();
		lock(&p->lk);
		goto retry;
	}

	/* No freelist cache when memory is very low */
	if(palloc.freecount < swapalloc.highwater) {
		unlock(&palloc.lk);
		uncachepage(p);
		return 1;
	}

	color = getpgcolor(p->va);
	for(np = palloc.head; np; np = np->next)
		if(np->color == color)
			break;

	/* No page of the correct color */
	if(np == 0) {
		unlock(&palloc.lk);
		uncachepage(p);
		return 1;
	}

	pageunchain(np);
	pagechaintail(np);
/*
* XXX - here's a bug? - np is on the freelist but it's not really free.
* when we unlock palloc someone else can come in, decide to
* use np, and then try to lock it.  they succeed after we've 
* run copypage and cachepage and unlock(&np->lk).  then what?
* they call pageunchain before locking(np), so it's removed
* from the freelist, but still in the cache because of
* cachepage below.  if someone else looks in the cache
* before they remove it, the page will have a nonzero ref
* once they finally lock(&np->lk).
*/
	lock(&np->lk);
	unlock(&palloc.lk);

	/* Cache the new version */
	uncachepage(np);
	np->va = p->va;
	np->daddr = p->daddr;
	copypage(p, np);
	cachepage(np, p->image);
	unlock(&np->lk);
	uncachepage(p);

	return 0;
}

void
copypage(Page *f, Page *t)
{
	KMap *ks, *kd;

	ks = kmap(f);
	kd = kmap(t);
	memmove((void*)VA(kd), (void*)VA(ks), BY2PG);
	kunmap(ks);
	kunmap(kd);
}

void
uncachepage(Page *p)			/* Always called with a locked page */
{
	Page **l, *f;

	if(p->image == 0)
		return;

	lock(&palloc.hashlock);
	l = &pghash(p->daddr);
	for(f = *l; f; f = f->hash) {
		if(f == p) {
			*l = p->hash;
			break;
		}
		l = &f->hash;
	}
	unlock(&palloc.hashlock);
	putimage(p->image);
	p->image = 0;
	p->daddr = 0;
}

void
cachepage(Page *p, Image *i)
{
	Page **l;

	/* If this ever happens it should be fixed by calling
	 * uncachepage instead of panic. I think there is a race
	 * with pio in which this can happen. Calling uncachepage is
	 * correct - I just wanted to see if we got here.
	 */
	if(p->image)
		panic("cachepage");

	incref(&i->ref);
	lock(&palloc.hashlock);
	p->image = i;
	l = &pghash(p->daddr);
	p->hash = *l;
	*l = p;
	unlock(&palloc.hashlock);
}

void
cachedel(Image *i, ulong daddr)
{
	Page *f, **l;

	lock(&palloc.hashlock);
	l = &pghash(daddr);
	for(f = *l; f; f = f->hash) {
		if(f->image == i && f->daddr == daddr) {
			lock(&f->lk);
			if(f->image == i && f->daddr == daddr){
				*l = f->hash;
				putimage(f->image);
				f->image = 0;
				f->daddr = 0;
			}
			unlock(&f->lk);
			break;
		}
		l = &f->hash;
	}
	unlock(&palloc.hashlock);
}

Page *
lookpage(Image *i, ulong daddr)
{
	Page *f;

	lock(&palloc.hashlock);
	for(f = pghash(daddr); f; f = f->hash) {
		if(f->image == i && f->daddr == daddr) {
			unlock(&palloc.hashlock);

			lock(&palloc.lk);
			lock(&f->lk);
			if(f->image != i || f->daddr != daddr) {
				unlock(&f->lk);
				unlock(&palloc.lk);
				return 0;
			}
			if(++f->ref == 1)
				pageunchain(f);
			unlock(&palloc.lk);
			unlock(&f->lk);

			return f;
		}
	}
	unlock(&palloc.hashlock);

	return 0;
}

Pte*
ptecpy(Pte *old)
{
	Pte *new;
	Page **src, **dst;

	new = ptealloc();
	dst = &new->pages[old->first-old->pages];
	new->first = dst;
	for(src = old->first; src <= old->last; src++, dst++)
		if(*src) {
			if(onswap(*src))
				dupswap(*src);
			else {
				lock(&(*src)->lk);
				(*src)->ref++;
				unlock(&(*src)->lk);
			}
			new->last = dst;
			*dst = *src;
		}

	return new;
}

Pte*
ptealloc(void)
{
	Pte *new;

	new = smalloc(sizeof(Pte));
	new->first = &new->pages[PTEPERTAB];
	new->last = new->pages;
	return new;
}

void
freepte(Segment *s, Pte *p)
{
	int ref;
	void (*fn)(Page*);
	Page *pt, **pg, **ptop;

	switch(s->type&SG_TYPE) {
	case SG_PHYSICAL:
		fn = s->pseg->pgfree;
		ptop = &p->pages[PTEPERTAB];
		if(fn) {
			for(pg = p->pages; pg < ptop; pg++) {
				if(*pg == 0)
					continue;
				(*fn)(*pg);
				*pg = 0;
			}
			break;
		}
		for(pg = p->pages; pg < ptop; pg++) {
			pt = *pg;
			if(pt == 0)
				continue;
			lock(&pt->lk);
			ref = --pt->ref;
			unlock(&pt->lk);
			if(ref == 0)
				free(pt);
		}
		break;
	default:
		for(pg = p->first; pg <= p->last; pg++)
			if(*pg) {
				putpage(*pg);
				*pg = 0;
			}
	}
	free(p);
}

ulong
pagenumber(Page *p)
{
	return p-palloc.pages;
}

void
checkpagerefs(void)
{
	int s;
	ulong i, np, nwrong;
	ulong *ref;
	
	np = palloc.user;
	ref = malloc(np*sizeof ref[0]);
	if(ref == nil){
		print("checkpagerefs: out of memory\n");
		return;
	}
	
	/*
	 * This may not be exact if there are other processes
	 * holding refs to pages on their stacks.  The hope is
	 * that if you run it on a quiescent system it will still
	 * be useful.
	 */
	s = splhi();
	lock(&palloc.lk);
	countpagerefs(ref, 0);
	portcountpagerefs(ref, 0);
	nwrong = 0;
	for(i=0; i<np; i++){
		if(palloc.pages[i].ref != ref[i]){
			iprint("page %#.8lux ref %d actual %lud\n", 
				palloc.pages[i].pa, palloc.pages[i].ref, ref[i]);
			ref[i] = 1;
			nwrong++;
		}else
			ref[i] = 0;
	}
	countpagerefs(ref, 1);
	portcountpagerefs(ref, 1);
	iprint("%lud mistakes found\n", nwrong);
	unlock(&palloc.lk);
	splx(s);
}

void
portcountpagerefs(ulong *ref, int print)
{
	ulong i, j, k, ns, n;
	Page **pg, *entry;
	Proc *p;
	Pte *pte;
	Segment *s;

	/*
	 * Pages in segments.  s->mark avoids double-counting.
	 */
	n = 0;
	ns = 0;
	for(i=0; i<conf.nproc; i++){
		p = proctab(i);
		for(j=0; j<NSEG; j++){
			s = p->seg[j];
			if(s)
				s->mark = 0;
		}
	}
	for(i=0; i<conf.nproc; i++){
		p = proctab(i);
		for(j=0; j<NSEG; j++){
			s = p->seg[j];
			if(s == nil || s->mark++)
				continue;
			ns++;
			for(k=0; k<s->mapsize; k++){
				pte = s->map[k];
				if(pte == nil)
					continue;
				for(pg = pte->first; pg <= pte->last; pg++){
					entry = *pg;
					if(pagedout(entry))
						continue;
					if(print){
						if(ref[pagenumber(entry)])
							iprint("page %#.8lux in segment %#p\n", entry->pa, s);
						continue;
					}
					if(ref[pagenumber(entry)]++ == 0)
						n++;
				}
			}
		}
	}
	if(!print){
		iprint("%lud pages in %lud segments\n", n, ns);
		for(i=0; i<conf.nproc; i++){
			p = proctab(i);
			for(j=0; j<NSEG; j++){
				s = p->seg[j];
				if(s == nil)
					continue;
				if(s->ref.ref != s->mark){
					iprint("segment %#.8lux (used by proc %lud pid %lud) has bad ref count %lud actual %lud\n",
						s, i, p->pid, s->ref, s->mark);
				}
			}
		}
	}
}


#include	"u.h"
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"error.h"

#include	"netif.h"

typedef struct Ram	Ram;
struct Ram
{
	QLock lk;
	Ram	*next;
	int	ref;
	/* simple for now */
	unsigned char **pages;
	int pagecount;
	int size;
	int	qref[2];
	ulong	path;
};

struct
{
	Lock lk;
	ulong	path;
} ramalloc;

enum
{
	Qdir,
	Qdata0,
	Qctl,
};

Dirtab ramdir[] =
{
	".",		{Qdir,0,QTDIR},	0,		DMDIR|0500,
	"data",		{Qdata0},	0,		0600,
	"ctl",	{Qctl},	0,		0600,
};
#define NPIPEDIR 3

static void
raminit(void)
{
}

/*
 *  create a ram, no streams are created until an open
 */
static Chan*
ramattach(char *spec)
{
	Ram *p;
	Chan *c;

	c = devattach('R', spec);
	p = malloc(sizeof(Ram));
	if(p == 0)
		exhausted("memory");
	p->ref = 1;
	p->size = 0;
	p->pagecount = 1;
	p->pages = mallocz(sizeof(char *), 1);
	p->pages[0] = mallocz(BY2PG, 1);
	lock(&ramalloc.lk);
	p->path = ++ramalloc.path;
	unlock(&ramalloc.lk);

	mkqid(&c->qid, NETQID(2*p->path, Qdir), 0, QTDIR);
	c->aux = p;
	c->dev = 0;
	return c;
}

static int
ramgen(Chan *c, char *name, Dirtab *tab, int ntab, int i, Dir *dp)
{
	Qid q;
	int len;
	Ram *p;

	if(i == DEVDOTDOT){
		devdir(c, c->qid, "#R", 0, eve, DMDIR|0555, dp);
		return 1;
	}
	i++;	/* skip . */
	if(tab==0 || i>=ntab)
		return -1;

	tab += i;
	p = c->aux;
	switch((ulong)tab->qid.path){
	case Qdata0:
		len = p->size;
		break;
	case Qctl:
		len = 0;
		break;
	default:
		len = tab->length;
		break;
	}
	mkqid(&q, NETQID(NETID(c->qid.path), tab->qid.path), 0, QTFILE);
	devdir(c, q, tab->name, len, eve, tab->perm, dp);
	return 1;
}


static Walkqid*
ramwalk(Chan *c, Chan *nc, char **name, int nname)
{
	Walkqid *wq;
	Ram *p;

	wq = devwalk(c, nc, name, nname, ramdir, NPIPEDIR, ramgen);
	if(wq != nil && wq->clone != nil && wq->clone != c){
		p = c->aux;
		qlock(&p->lk);
		p->ref++;
		if(c->flag & COPEN){
			print("channel open in ramwalk\n");
			switch(NETTYPE(c->qid.path)){
			case Qdata0:
				p->qref[0]++;
				break;
			case Qctl:
				p->qref[1]++;
				break;
			}
		}
		qunlock(&p->lk);
	}
	return wq;
}

static int
ramstat(Chan *c, uchar *db, int n)
{
	Ram *p;
	Dir dir;

	p = c->aux;

	switch(NETTYPE(c->qid.path)){
	case Qdir:
		devdir(c, c->qid, ".", 0, eve, DMDIR|0555, &dir);
		break;
	case Qdata0:
		devdir(c, c->qid, "data", p->size, eve, 0600, &dir);
		break;
	case Qctl:
		devdir(c, c->qid, "ctl", 0, eve, 0600, &dir);
		break;
	default:
		panic("ramstat");
	}
	n = convD2M(&dir, db, n);
	if(n < BIT16SZ)
		error(Eshortstat);
	return n;
}

/*
 *  if the stream doesn't exist, create it
 */
static Chan*
ramopen(Chan *c, int omode)
{
	Ram *p;

	if(c->qid.type & QTDIR){
		if(omode != OREAD)
			error(Ebadarg);
		c->mode = omode;
		c->flag |= COPEN;
		c->offset = 0;
		return c;
	}

	p = c->aux;
	qlock(&p->lk);
	switch(NETTYPE(c->qid.path)){
	case Qdata0:
		p->qref[0]++;
		break;
	case Qctl:
		p->qref[1]++;
		break;
	}
	qunlock(&p->lk);

	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	c->iounit = qiomaxatomic;
	return c;
}

static void
ramclose(Chan *c)
{
	Ram *p;

	p = c->aux;
	qlock(&p->lk);

	if(c->flag & COPEN){
		switch(NETTYPE(c->qid.path)){
		case Qdata0:
			p->qref[0]--;
			break;
		case Qctl:
			p->qref[1]--;
			break;
		}
	}

	/*
	 *  free the structure on last close
	 */
	p->ref--;
	if(p->ref == 0){
		int i;
		qunlock(&p->lk);
		for(i = 0; i < p->pagecount; i++)
			free(p->pages[i]);
		free(p->pages);
		free(p);
	} else
		qunlock(&p->lk);
}

static long rampageread(Ram *p, void *va, long n, vlong offset)
{
	int i;
	long total = n, offinpage, leninpage;

	/* figure out what range we can actually read */
	if (offset > p->size)
		return 0;
	if (offset + n > p->size) 
		n = p->size - offset;
	/* granular copy */
	for(i = offset / BY2PG; n > 0; i++) {
		/* i is the page */
		offinpage = offset & (BY2PG - 1);
		leninpage = BY2PG - offinpage;
		/* unless there is too little left ... */
		if (leninpage > n)
			leninpage = n;
		memcpy(va, p->pages[i] + offinpage, leninpage);
		offset += offinpage;
		n -= leninpage; 
		va += leninpage;
	}
	return total;
}

static long
ramread(Chan *c, void *va, long n, vlong offset)
{
	Ram *p;
	char *buf, *s, *e;

	p = c->aux;

	switch(NETTYPE(c->qid.path)){
	case Qdir:
		return devdirread(c, va, n, ramdir, NPIPEDIR, ramgen);
	case Qdata0:
		return rampageread(p, va, n, offset);
	case Qctl:
		buf = smalloc(8192);
		s = buf;
		e = buf + 8192;
		s = seprint(s, e, "pages %p count %d ", p->pages, p->pagecount);
		seprint(s, e, "size %d\n", p->size);
		n = readstr(offset, va, n, buf);
		free(buf);
		return n;
	default:
		panic("ramread");
	}
	return -1;	/* not reached */
}

/* for the range offset .. offset + n, make sure we have pages */
static void pages(Ram *p, long n, vlong offset)
{
	int i;
	int newpagecount;
	unsigned char **newpages;
	newpagecount = (offset + n + BY2PG-1)/BY2PG;
	if (newpagecount > p->pagecount) {
		newpages = mallocz(sizeof(char *) * newpagecount, 1);
		if (! newpages)
			error("No more pages in devram");
		memcpy(newpages, p->pages, sizeof(char *) * p->pagecount);
		free(p->pages);
		p->pages = newpages;
		p->pagecount = newpagecount;
		/* now allocate them */
		for(i = offset / BY2PG; i < newpagecount; i++) {
			if (p->pages[i])
				continue;
			p->pages[i] = mallocz(BY2PG, 1);
		}
	}
}
static long rampagewrite(Ram *p, void *va, long n, vlong offset)
{
	int i;
	long total = n, offinpage, leninpage;
	long newsize;
	pages(p, n, offset);
	
	/* granular copy */
	newsize = offset + n;
	for(i = offset / BY2PG; n > 0; i++) {
		/* i is the page */
		offinpage = offset & (BY2PG - 1);
		leninpage = BY2PG - offinpage;
		/* unless there is too little left ... */
		if (leninpage > n)
			leninpage = n;
		memcpy(p->pages[i] + offinpage, va, leninpage);
		offset += leninpage;
		n -= leninpage; 
		va += leninpage;
	}
	p->size = newsize > p->size? newsize : p->size;
	return total;
}
static long
ramwrite(Chan *c, void *va, long n, vlong offset)
{
	Ram *p;

	if(!islo())
		print("ramwrite hi %lux\n", getcallerpc(&c));
	p = c->aux;
	switch(NETTYPE(c->qid.path)){
	case Qdata0:
		n = rampagewrite(p, va, n, offset);
		break;

	case Qctl:
		if (strcmp(va, "free") == 0) {
			int i;
			unsigned char **new = mallocz(sizeof(char *), 1);
			unsigned char *page = p->pages[0];
			for(i = 1; i < p->pagecount; i++)
				free(p->pages[i]);
			free(p->pages);
			p->pages = new;
			p->pages[0] = page;
			p->size = 0;
			p->pagecount = 1;
		} else {
			error("bad command");
		}
		break;

	default:
		panic("ramwrite");
	}

	return n;
}


Dev ramdevtab = {
	'R',
	"ram",

	devreset,
	raminit,
	devshutdown,
	ramattach,
	ramwalk,
	ramstat,
	ramopen,
	devcreate,
	ramclose,
	ramread,
	devbread,
	ramwrite,
	devbwrite,
	devremove,
	devwstat,
};

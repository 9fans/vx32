#include "u.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "ureg.h"
#include "error.h"

#include "sd.h"

void	loopdev(char*, int);

typedef struct Ctlr Ctlr;
struct Ctlr{
	Ctlr	*next;
	Ctlr	*prev;
	
	QLock	lk;
	SDev	*sdev;

	Chan	*c;
	int		mode;
	uvlong	qidpath;
	char		fn[20];
};

static	Lock	ctlrlock;
static	Ctlr	*ctlrhead;
static	Ctlr	*ctlrtail;

SDifc sdloopifc;

static void
loopopen(Ctlr *c)
{
	if(c->c == nil)
		c->c = namec(c->fn, Aopen, c->mode, 0);
}

static SDev*
looppnp(void)
{
	struct stat sbuf;
	char c, c2;
	char fn[20];

	for(c = 'a'; c <= 'j'; ++c){
		sprint(fn, "#Z/dev/sd%c", c);
		if(stat(fn+2, &sbuf) == 0)
			loopdev(fn, ORDWR);
	}
	for(c = '0'; c <= '9'; ++c){
		sprintf(fn, "#Z/dev/sd%c",c);
		if(stat(fn+2, &sbuf) == 0)
			loopdev(fn, ORDWR);
	}
	for(c = 'a'; c <= 'j'; ++c){
		sprint(fn, "#Z/dev/hd%c", c);
		if(stat(fn+2, &sbuf) == 0)
			loopdev(fn, ORDWR);
	}
	for(c = '0'; c <= '9'; ++c){
		sprint(fn, "#Z/dev/wd%c", c);
		if(stat(fn+2, &sbuf) == 0)
			loopdev(fn, ORDWR);
	}
	for(c = '0'; c <= '8'; ++c){
		for(c2 = '0'; c2 <= '8'; ++c2){
			sprint(fn, "#Z/dev/cciss/c%cd%c", c, c2);
			if(stat(fn+2, &sbuf) == 0)
				loopdev(fn, ORDWR);
		}
	}
	return nil;
}

/*
 * Cannot error.
 * Check that unit is available.
 * Return 1 if so, 0 if not.
 */
static int
loopverify(SDunit *u)
{	
	return 1;
}

/*
 * Cannot error.
 * Check that unit is online.
 * If media changed, return 2.
 * If ready, return 1.
 * If not ready, return 0.
 */
static int
looponline(SDunit *unit)
{
	uchar buf[sizeof(Dir)+100];
	Chan *c;
	SDev *sdev;
	Ctlr *ctlr;
	Dir dir;
	long n;
	
	if(waserror())
		return 0;

	sdev = unit->dev;
	ctlr = sdev->ctlr;
	loopopen(ctlr);
	c = ctlr->c;
	n = devtab[c->type]->stat(c, buf, sizeof buf);
	if(convM2D(buf, n, &dir, nil) == 0)
		error("internal error: stat error in looponline");
	if(ctlr->qidpath != dir.qid.path){
		unit->sectors = dir.length/512;
		unit->secsize = 512;
		ctlr->qidpath = dir.qid.path;
		poperror();
		return 2;
	}
	poperror();
	return 1;
}

static int
looprio(SDreq *r)
{
	SDev *sdev;
	SDunit *unit;
	Ctlr *ctlr;
	uchar *cmd;
	uvlong lba;
	long count, n;
	Chan *c;
	int status;

	unit = r->unit;
	sdev = unit->dev;
	ctlr = sdev->ctlr;
	loopopen(ctlr);
	cmd = r->cmd;

	if((status = sdfakescsi(r, nil, 0)) != SDnostatus){
#warning "Need to check for SDcheck in sdloop.";
		/* XXX check for SDcheck here */
		r->status = status;
		return status;
	}

	switch(cmd[0]){
	case 0x28:	/* read */
	case 0x2A:	/* write */
		break;
	default:
		print("%s: bad cmd 0x%.2ux\n", unit->perm.name, cmd[0]);
		r->status = SDcheck;
		return SDcheck;
	}

	lba = (cmd[2]<<24)|(cmd[3]<<16)|(cmd[4]<<8)|cmd[5];
	count = (cmd[7]<<8)|cmd[8];
	if(r->data == nil)
		return SDok;
	if(r->dlen < count*512)
		count = r->dlen/512;

	c = ctlr->c;
	if(cmd[0] == 0x28)
		n = devtab[c->type]->read(c, r->data, count*512, lba*512);
	else
		n = devtab[c->type]->write(c, r->data, count*512, lba*512);
	r->rlen = n;
	return SDok;
}

static int
looprctl(SDunit *unit, char *p, int l)
{
	Ctlr *ctlr;
	char *e, *op;
	
	ctlr = unit->dev->ctlr;
	loopopen(ctlr);
	e = p+l;
	op = p;
	
	p = seprint(p, e, "loop %s %s\n", ctlr->mode == ORDWR ? "rw" : "ro", chanpath(ctlr->c));
	p = seprint(p, e, "geometry %llud 512\n", unit->sectors*512);
	return p - op;
}

static int
loopwctl(SDunit *u, Cmdbuf *cmd)
{
	cmderror(cmd, Ebadarg);
	return 0;
}

static void
loopclear1(Ctlr *ctlr)
{
	lock(&ctlrlock);
	if(ctlr->prev)
		ctlr->prev->next = ctlr->next;
	else
		ctlrhead = ctlr;
	if(ctlr->next)
		ctlr->next->prev = ctlr->prev;
	else
		ctlrtail = ctlr->prev;
	unlock(&ctlrlock);
	
	if(ctlr->c)
		cclose(ctlr->c);
	free(ctlr);
}

static void
loopclear(SDev *sdev)
{
	loopclear1(sdev->ctlr);
}

static char*
looprtopctl(SDev *s, char *p, char *e)
{
	Ctlr *c;
	char *r;

	c = s->ctlr;
	loopopen(c);
	r = "ro";
	if(c->mode == ORDWR)
		r = "rw";
	return seprint(p, e, "%s loop %s %s\n", s->name, r, chanpath(c->c));
}

static int
loopwtopctl(SDev *sdev, Cmdbuf *cb)
{
	int mode;

	mode = 0;
	if(cb->nf != 2)
		cmderror(cb, Ebadarg);
	if(strcmp(cb->f[0], "rw") == 0)
		mode = ORDWR;
	else if(strcmp(cb->f[0], "ro") == 0)
		mode = OREAD;
	else
		cmderror(cb, Ebadarg);
	
	loopdev(cb->f[1], mode);
	return 0;
}

void
loopdev(char *name, int mode)
{
	Chan *c;
	Ctlr *volatile ctlr;
	SDev *volatile sdev;

	ctlr = nil;
	sdev = nil;
/*
	if(waserror()){
		cclose(c);
		if(ctlr)
			free(ctlr);
		if(sdev)
			free(sdev);
		nexterror();
	}
*/

	ctlr = smalloc(sizeof *ctlr);
	sdev = smalloc(sizeof *sdev);
	sdev->ifc = &sdloopifc;
	sdev->ctlr = ctlr;
	sdev->nunit = 1;
	sdev->idno = '0';
	ctlr->sdev = sdev;
	strcpy(ctlr->fn, name);
	ctlr->mode = mode;
/*
	poperror();
*/

	lock(&ctlrlock);
	ctlr->next = nil;
	ctlr->prev = ctlrtail;
	ctlrtail = ctlr;
	if(ctlr->prev)
		ctlr->prev->next = ctlr;
	else
		ctlrhead = ctlr;
	unlock(&ctlrlock);
	
	sdadddevs(sdev);
}


SDifc sdloopifc = {
	"loop",

	looppnp,
	nil,		/* legacy */
	nil,		/* enable */
	nil,		/* disable */

	loopverify,
	looponline,
	looprio,
	looprctl,
	loopwctl,

	scsibio,
	nil,	/* probe */
	loopclear,	/* clear */
	looprtopctl,
	loopwtopctl,
};




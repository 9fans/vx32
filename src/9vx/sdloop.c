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
};

static	Lock	ctlrlock;
static	Ctlr	*ctlrhead;
static	Ctlr	*ctlrtail;

SDifc sdloopifc;

static SDev*
looppnp(void)
{
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
	cmd = r->cmd;

#if 0	
	if((status = sdfakescsi(r, ctlr->info, sizeof ctlr->info)) != SDnostatus){
		/* XXX check for SDcheck here */
		r->status = status;
		return status;
	}
#endif

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
	e = p+l;
	op = p;
	
	p = seprint(p, e, "loop %s %s\n", ctlr->mode == ORDWR ? "rw" : "ro", chanpath(ctlr->c));
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
	
	cclose(ctlr->c);
	free(ctlr);
}

static void
loopclear(SDev *sdev)
{
	loopclear1(sdev->ctlr);
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

	c = namec(name, Aopen, mode, 0);
	ctlr = nil;
	sdev = nil;
	if(waserror()){
		cclose(c);
		if(ctlr)
			free(ctlr);
		if(sdev)
			free(sdev);
		nexterror();
	}

	ctlr = smalloc(sizeof *ctlr);
	sdev = smalloc(sizeof *sdev);
	sdev->ifc = &sdloopifc;
	sdev->ctlr = ctlr;
	sdev->nunit = 1;
	sdev->idno = '0';
	ctlr->sdev = sdev;
	ctlr->c = c;
	ctlr->mode = mode;
	poperror();

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
	nil,
	loopwtopctl,
};

SDifc *sdifc[] = 
{
	&sdloopifc,
	nil
};




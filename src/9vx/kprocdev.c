/*
 * Defer all interactions with a device into a kproc.
 * It is not okay for cpu0 (the one that runs user code
 * and thus user system calls) to block in a host OS system call.
 * Any device that might do so needs to be wrapped using
 * makekprocdev, so that all the actual Dev callbacks happen
 * inside a kproc running on a different cpu (pthread).
 */

#include "u.h"
#include <pthread.h>
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

int tracekdev = 0;

static Dev *kdev;
static int nkdev;

enum
{
	CallBread = 1,
	CallBwrite,
	CallClose,
	CallCreate,
	CallOpen,
	CallRead,
	CallRemove,
	CallStat,
	CallWalk,
	CallWrite,
	CallWstat
};
typedef struct Kcall Kcall;
struct Kcall
{
	int call;
	int done;
	Proc *p;
	char err[ERRMAX];
	char note[ERRMAX];
	pthread_t pthread;

	Chan *c;
	Chan *nc;
	long n;
	vlong off;
	void *a;
	int mode;
	ulong perm;
	char *name;
	Walkqid *wq;
	char **wname;
	int nwname;
	Block *b;
};

typedef struct Kserve Kserve;
struct Kserve
{
	Rendez r;
	Kserve *next;
	Kcall *kc;
	Proc *p;
};

static struct {
	Lock lk;
	Kserve *servers;
	Kcall *calls;
} kstate;

static void
kserve(Kcall *kc)
{
	Dev *d;

	/* todo: change identity */

	d = &kdev[kc->c->type];
	switch(kc->call){
	default:
		snprint(kc->err, sizeof kc->err, "unknown call %d", kc->call);
		return;
	case CallWalk:
		kc->wq = d->walk(kc->c, kc->nc, kc->wname, kc->nwname);
		break;
	case CallStat:
		kc->n = d->stat(kc->c, kc->a, kc->n);
		break;
	case CallWstat:
		kc->n = d->wstat(kc->c, kc->a,kc->n);
		break;
	case CallOpen:
		kc->nc = d->open(kc->c, kc->mode);
		break;
	case CallCreate:
		d->create(kc->c, kc->name, kc->mode, kc->perm);
		break;
	case CallRead:
		kc->n = d->read(kc->c, kc->a, kc->n, kc->off);
		break;
	case CallWrite:
		kc->n = d->write(kc->c, kc->a, kc->n, kc->off);
		break;
	case CallClose:
		d->close(kc->c);
		break;
	case CallRemove:
		d->remove(kc->c);
		break;
	case CallBread:
		kc->b = d->bread(kc->c, kc->n, kc->off);
		break;
	case CallBwrite:
		kc->n = d->bwrite(kc->c, kc->b, kc->off);
		break;
	}	
}

static int
havekc(void *v)
{
	Kserve *s;
	
	s = v;
	return s->kc != nil;
}

static void
kserver(void *v)
{
	Kserve *s;
	Kcall *kc;

	s = v;
	s->p = up;
	for(;;){
		/* wait for request */
		while(s->kc == nil){
			sleep(&s->r, havekc, s);
		}

		/* serve request */
		kc = s->kc;
		if(tracekdev)
			print("kserver %ld has %ld %s [%p %p]\n",
				up->pid, kc->p->pid, kc->p->text, kc, kc->c);
		s->kc = nil;
		if(!waserror()){
			kc->pthread = pthread_self();
			kserve(kc);
			kc->pthread = 0;
			poperror();
		}else
			strcpy(kc->err, up->errstr);
		kc->done = 1;
		wakeup(&kc->p->sleep);
	}
}

static int
donekc(void *v)
{
	Kcall *kc;
	
	kc = v;
	return kc->done;
}

void
kcall(Kcall *kc, int call)
{
	Kserve *s;
	pthread_t p;

	kc->call = call;
	kc->p = up;

	if(up->kp){
		/* already in a kproc; call directly. */
		kserve(kc);
		return;
	}

	/* acquire server */
	lock(&kstate.lk);
	if(kstate.servers){
		s = kstate.servers;
		kstate.servers = s->next;
		s->kc = kc;
		if(tracekdev)
			print("kcall %ld %s has %ld [%p %p]\n",
				up->pid, up->text, s->p->pid, kc, kc->c);
		wakeup(&s->r);
	}else{
		s = malloc(sizeof *s);
		s->kc = kc;
		if(tracekdev)
			print("kcall %ld %s forks new server\n", up->pid, up->text);
		kproc("*io*", kserver, s);
	}
	unlock(&kstate.lk);

	while(waserror()){
		strcpy(kc->note, up->errstr);
		p = kc->pthread;
		if(!kc->done && p)
			pthread_kill(p, SIGUSR1);
	}
	while(!kc->done)
		sleep(&up->sleep, donekc, kc);
	poperror();

	if(tracekdev)
		print("kcall %ld %s releases %ld\n",
			up->pid, up->text, s->p->pid);
	/* release server */
	lock(&kstate.lk);
	s->next = kstate.servers;
	kstate.servers = s;
	unlock(&kstate.lk);

	if(kc->err[0])
		error(kc->err);
}

static Walkqid*
kdevwalk(Chan *c, Chan *nc, char **name, int nname)
{
	Kcall kc;

	// Okay to pass name pointers, because they
	// are kernel-allocated strings.

	memset(&kc, 0, sizeof kc);
	kc.c = c;
	kc.nc = nc;
	kc.wname = name;
	kc.nwname = nname;
	kcall(&kc, CallWalk);
	return kc.wq;
}

static int
kdevstat(Chan *c, uchar *a, int n)
{
	Kcall kc;
	uchar *buf;

	/*
	 * Have to copy in case a is a user pointer -- the
	 * kproc doesn't run in the address space of up.
	 * TODO: Don't copy if a is a kernel pointer.
	 */
	buf = smalloc(n);
	if(waserror()){
		free(buf);
		nexterror();
	}

	memset(&kc, 0, sizeof kc);
	kc.c = c;
	kc.a = buf;
	kc.n = n;
	kcall(&kc, CallStat);
	memmove(a, buf, kc.n);
	poperror();
	free(buf);
	return kc.n;
}

static Chan*
kdevopen(Chan *c, int mode)
{
	Kcall kc;
	
	memset(&kc, 0, sizeof kc);
	kc.c = c;
	kc.mode = mode;
	kcall(&kc, CallOpen);
	return kc.nc;
}

static void
kdevcreate(Chan *c, char *name, int mode, ulong perm)
{
	Kcall kc;
	
	memset(&kc, 0, sizeof kc);
	kc.c = c;
	kc.name = name;
	kc.mode = mode;
	kc.perm = perm;
	kcall(&kc, CallCreate);
}

static void
kdevclose(Chan *c)
{
	Kcall kc;
	
	memset(&kc, 0, sizeof kc);
	kc.c = c;
	kcall(&kc, CallClose);
}

static long
kdevread(Chan *c, void *a, long n, vlong off)
{
	Kcall kc;
	uchar *buf;

	/*
	 * Have to copy in case a is a user pointer -- the
	 * kproc doesn't run in the address space of up.
	 * TODO: Don't copy if a is a kernel pointer.
	 */
	buf = smalloc(n);
	if(waserror()){
		free(buf);
		nexterror();
	}

	memset(&kc, 0, sizeof kc);
	kc.c = c;
	kc.a = buf;
	kc.n = n;
	kc.off = off;
	kcall(&kc, CallRead);
	memmove(a, buf, kc.n);
	poperror();
	free(buf);
	return kc.n;
}

static long
kdevwrite(Chan *c, void *a, long n, vlong off)
{
	Kcall kc;
	uchar *buf;

	/*
	 * Have to copy in case a is a user pointer -- the
	 * kproc doesn't run in the address space of up.
	 * TODO: Don't copy if a is a kernel pointer.
	 */
	buf = smalloc(n);
	if(waserror()){
		free(buf);
		nexterror();
	}

	memmove(buf, a, n);
	memset(&kc, 0, sizeof kc);
	kc.c = c;
	kc.a = buf;
	kc.n = n;
	kc.off = off;
	kcall(&kc, CallWrite);
	poperror();
	free(buf);
	return kc.n;
}

static void
kdevremove(Chan *c)
{
	Kcall kc;
	
	memset(&kc, 0, sizeof kc);
	kc.c = c;
	kcall(&kc, CallRemove);
}

static int
kdevwstat(Chan *c, uchar *a, int n)
{
	Kcall kc;
	uchar *buf;
	
	/*
	 * Have to copy in case a is a user pointer -- the
	 * kproc doesn't run in the address space of up.
	 * TODO: Don't copy if a is a kernel pointer.
	 */
	buf = smalloc(n);
	if(waserror()){
		free(buf);
		nexterror();
	}	
	memmove(buf, a, n);
	memset(&kc, 0, sizeof kc);
	kc.c = c;
	kc.a = buf;
	kc.n = n;
	kcall(&kc, CallWstat);
	poperror();
	free(buf);
	return kc.n;
}

static Block*
kdevbread(Chan *c, long n, ulong offset)
{
	Kcall kc;

	memset(&kc, 0, sizeof kc);
	kc.c = c;
	kc.n = n;
	kc.off = offset;
	kcall(&kc, CallBread);
	return kc.b;
}

static long
kdevbwrite(Chan *c, Block *bp, ulong offset)
{
	Kcall kc;
	
	memset(&kc, 0, sizeof kc);
	kc.c = c;
	kc.b = bp;
	kc.off = offset;
	kcall(&kc, CallBwrite);
	return kc.n;
}

void
makekprocdev(Dev *d)
{
	int i;
	
	if(kdev == nil){
		for(nkdev=0; devtab[nkdev]; nkdev++)
			;
		kdev = malloc(nkdev*sizeof kdev[0]);
	}
	
	for(i=0; devtab[i] && devtab[i] != d; i++)
		;
	if(devtab[i] == nil)
		panic("kdevinit");
	kdev[i] = *d;

	d->walk = kdevwalk;
	d->stat = kdevstat;
	d->open = kdevopen;
	d->create = kdevcreate;
	d->close = kdevclose;
	d->read = kdevread;
	d->bread = kdevbread;
	d->write = kdevwrite;
	d->bwrite = kdevbwrite;
	d->remove = kdevremove;
	d->wstat = kdevwstat;
}


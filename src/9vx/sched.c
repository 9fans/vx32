/*
 * Plan 9 VX scheduler
 *
 * Allocate new processors (host threads) for each kproc.
 * Everyone else gets scheduled via the regular scheduler
 * on cpu0.  It is tempting to use multiple threads and 
 * multiple vx32 instances on an SMP to get parallelism of
 * user execution, but we can't: there's only one user address space.
 * (The kprocs are okay because they never touch user memory.)
 */

#define	WANT_M

#include	"u.h"
#include	<pthread.h>
#include	<sys/poll.h>
#include	<sched.h>
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"trace.h"

/*
 * The cpu0 scheduler calls idlehands when there is
 * nothing left on the main runqueue (runproc
 * is returning nil).  Instead of chewing up the
 * host CPU spinning, we go to sleep using pthreads,
 * but then if some other kproc readies a normal
 * proc, it needs to call noidlehands to kick cpu0.
 */
static int idlewakeup;
static Psleep idling;

void
idlehands(void)
{
	int nbad;

	plock(&idling);
	nbad = 0;
	while(!idlewakeup){
		if(traceprocs)
			iprint("cpu%d: idlehands\n", m->machno);
		psleep(&idling);
		if(traceprocs)
			iprint("cpu%d: busy hands\n", m->machno);
		if(!idlewakeup && ++nbad%1000 == 0)
			iprint("idlehands spurious wakeup\n");
	}
	idlewakeup = 0;
	if(traceprocs)
		iprint("cpu%d: idlehands returning\n", m->machno);
	punlock(&idling);
}

void
noidlehands(void)
{
	if(m->machno == 0)
		return;
	plock(&idling);
	idlewakeup = 1;
	pwakeup(&idling);
	punlock(&idling);
}

/*
 * Special run queue for kprocs.
 */
static Schedq kprocq;
static int nrunproc;
static Psleep run;

/*
 * Ready the proc p. 
 * If it's a normal proc, it goes to the normal scheduler.
 * Otherwise it gets put on the kproc run queue, and
 * maybe a new "cpu" gets forked to run the kproc.
 */
void
ready(Proc *p)
{
	if(p->kp == 0){
		_ready(p);
		noidlehands();	/* kick cpu0 if it is sleeping */
		return;
	}
	plock(&run);
	lock(&kprocq.lk);	/* redundant but fine */
	p->state = Ready;
	p->rnext = 0;
	if(kprocq.tail)
		kprocq.tail->rnext = p;
	else
		kprocq.head = p;
	kprocq.tail = p;
	/*
	 * If there are more kprocs on the queue
	 * than there are cpus waiting to run kprocs,
	 * kick off a new one.
	 */
	kprocq.n++;
	if(kprocq.n > nrunproc){
		if(traceprocs)
			iprint("create new cpu: kprocq.n=%d nrunproc=%d\n", kprocq.n, nrunproc);
		nrunproc++;
		newmach();
	}
	if(traceprocs)
		iprint("cpu%d: ready %ld %s; wakeup kproc cpus\n", m->machno, p->pid, p->text);
	pwakeup(&run);
	unlock(&kprocq.lk);
	punlock(&run);
}

/*
 * Get a new proc to run.
 * If we're running on cpu0, use the normal scheduler
 * to get a normal proc.
 */
Proc*
runproc(void)
{
	int nbad;
	Proc *p;

	if(m->machno == 0)
		return _runproc();

	nbad = 0;
	plock(&run);
	lock(&kprocq.lk);	/* redundant but fine */
	if(m->new){
		nrunproc--;
		m->new = 0;
	}
	while((p = kprocq.head) == nil){
		nrunproc++;
		unlock(&kprocq.lk);
		if(traceprocs)
			iprint("cpu%d: runproc psleep %d %d\n", m->machno, kprocq.n, nrunproc);
		psleep(&run);
		lock(&kprocq.lk);
		if(kprocq.head == nil && ++nbad%1000 == 0)
			iprint("cpu%d: runproc spurious wakeup\n", m->machno);	
		if(traceprocs)
			iprint("cpu%d: runproc awake\n", m->machno);
		nrunproc--;
	}
	kprocq.head = p->rnext;
	if(kprocq.head == 0)
		kprocq.tail = nil;
	kprocq.n--;
	if(traceprocs)
		iprint("cpu%d: runproc %ld %s [%d %d]\n",
			m->machno, p->pid, p->text, kprocq.n, nrunproc);
	unlock(&kprocq.lk);
	punlock(&run);
	/*
	 * To avoid the "double sleep" bug
	 * Full history begins at:
	 * http://9fans.net/archive/2010/06/71
	 * Who knows where it will end
	 */
	while (p->mach)
		sched_yield();
	return p;
}

/*
 * Limit CPU usage going to sleep while holding the run lock
 */
void
plimitproc(void *v)
{
	int lim;
	uint sleeping, working;

	lim = *((int*)v);
	sleeping = 100000 * (100 - lim) / 100;
	working = 100000 * lim / 100;

	for(;;){
		usleep(working);
		plock(&run);
		usleep(sleeping);
		punlock(&run);
	}
}

/*
 * Host OS process sleep and wakeup.
 */
static pthread_mutex_t initmutex = PTHREAD_MUTEX_INITIALIZER;

struct Pwaiter
{
	pthread_cond_t cond;
	Pwaiter *next;
	int awake;
};

void
__plock(Psleep *p)
{
	int r;

	if(!p->init){
		if((r = pthread_mutex_lock(&initmutex)) != 0)
			panic("pthread_mutex_lock initmutex: %d", r);
		if(!p->init){
			p->init = 1;
			pthread_mutex_init(&p->mutex, nil);
		}
		if((r = pthread_mutex_unlock(&initmutex)) != 0)
			panic("pthread_mutex_unlock initmutex: %d", r);
	}
	if((r = pthread_mutex_lock(&p->mutex)) != 0)
		panic("pthread_mutex_lock: %d", r);
}

void
__punlock(Psleep *p)
{
	int r;

	if((r = pthread_mutex_unlock(&p->mutex)) != 0)
		panic("pthread_mutex_unlock: %d", r);
}

void
__psleep(Psleep *p)
{
	int r;
	Pwaiter w;

	memset(&w, 0, sizeof w);
	pthread_cond_init(&w.cond, nil);
	w.next = p->waiter;
	p->waiter = &w;
	while(!w.awake)
		if((r = pthread_cond_wait(&w.cond, &p->mutex)) != 0)
			panic("pthread_cond_wait: %d", r);
	pthread_cond_destroy(&w.cond);
}

void
__pwakeup(Psleep *p)
{
	int r;
	Pwaiter *w;

	w = p->waiter;
	if(w){
		p->waiter = w->next;
		w->awake = 1;
		if((r = pthread_cond_signal(&w->cond)) != 0)
			panic("pthread_cond_signal: %d", r);
	}
}


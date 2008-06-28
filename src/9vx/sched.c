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
#include	<sched.h>
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"trace.h"

/* Pthreads-based sleep and wakeup. */
typedef struct Psleep Psleep;
struct Psleep
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

static void
plock(Psleep *p)
{
	pthread_mutex_lock(&p->mutex);
}

static void
punlock(Psleep *p)
{
	pthread_mutex_unlock(&p->mutex);
}

static void
psleep(Psleep *p)
{
	/*
	 * OS X is trying to be helpful and has changed the behavior
	 * of pthreads condition variables.  After pthread_cond_signal
	 * any subsequent pthread_cond_wait returns immediately.
	 * This is perhaps more sensible behavior than the standard,
	 * but it's not actually what the standard requires.
	 * So we have to pthread_cond_init to clear any pre-existing
	 * condition.  This is okay because we hold the lock that
	 * protects the condition in the first place.  Sigh.
	 */
	pthread_cond_init(&p->cond, nil);
	pthread_cond_wait(&p->cond, &p->mutex);
}

static void
pwakeup(Psleep *p)
{
	pthread_cond_signal(&p->cond);
}


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
	plock(&idling);
	while(!idlewakeup){
		psleep(&idling);
		iprint("idlehands spurious wakeup\n");
	}
	idlewakeup = 0;
	punlock(&idling);
}

void
noidlehands(void)
{
	if(m->machno == 0)
		return;
	plock(&idling);
	idlewakeup++;
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
	if(kprocq.n > nrunproc)
		newmach();
iprint("ready p\n");
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
	while((p = kprocq.head) == nil){
		nrunproc++;
		unlock(&kprocq.lk);
		psleep(&run);
		lock(&kprocq.lk);
		if(kprocq.head == nil && ++nbad%1000 == 0)
			iprint("cpu%d: runproc spurious wakeup\n", m->machno);	
		nrunproc--;
	}
	kprocq.head = p->rnext;
	if(kprocq.head == 0)
		kprocq.tail = nil;
	kprocq.n--;
	unlock(&kprocq.lk);
	punlock(&run);
	return p;
}


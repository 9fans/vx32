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
		psleep(&idling);
		if(!idlewakeup && ++nbad%1000 == 0)
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

/*
 * Host OS process sleep and wakeup.
 * This is complicated.
 *
 * Ideally, we'd just use a single pthread_cond_t, have everyone
 * pthread_cond_wait on it, and use pthread_cond_signal
 * to wake people up.  Unfortunately, that fails miserably
 * on OS X: sometimes the wakeups just plain get missed.
 * Perhaps it has something to do with all the signals that
 * are flying around.
 *
 * To work around the OS X pthreads problems, there is a
 * second implementation turned on by #defining PIPES to 1.
 * This implementation uses a pipe and reads and writes bytes
 * from the pipe to implement sleep and wakeup.  Perhaps not
 * surprisingly, the naive implementation of this hangs:
 * reads miss writes.  Instead, the actual implementation uses
 * select to poll whether the read would succeed, and once a
 * second it tries the read even if select doesn't think it will.
 * This timeout lets us make progress when an event gets missed
 * (happens only rarely).  This is enough to get things going on
 * OS X.
 *
 * On my Athlon 64 running Linux,
 * time to run mk -a in /sys/src/9/pc:
 *
 * 	90s	default implementation (one pthread_cond_t)
 * 	85s	WAITERS (pthread_cond_t for each waiter)
 * 	88s	PIPES
 *
 * I implemented per-thread pthread_cond_t's to see if they
 * were any faster on non-OS X systems, but I can't see any
 * difference.  Running the WAITERS version on OS X causes
 * mysterious crashes.  I'm thoroughly confused.  
 */
#define	PIPES	0
#define	WAITERS	1

#ifdef __APPLE__
#undef	PIPES
#define	PIPES	1
#undef	WAITERS
#define	WAITERS	0
#endif

struct Pwaiter
{
	pthread_cond_t cond;
	Pwaiter *next;
	int awake;
};

void
plock(Psleep *p)
{
	pthread_mutex_lock(&p->mutex);
	if(!p->condinit){
		p->condinit = 1;
		pthread_cond_init(&p->cond, nil);
	}
#if PIPES
	if(p->fd[1] == 0){
		pipe(p->fd);
		fcntl(p->fd[0], F_SETFL, fcntl(p->fd[0], F_GETFL)|O_NONBLOCK);
	}
#endif
}

void
punlock(Psleep *p)
{
	pthread_mutex_unlock(&p->mutex);
}

void
psleep(Psleep *p)
{
#if PIPES
	p->nread++;
	punlock(p);
	char c;
	while(read(p->fd[0], &c, 1) < 1){
		struct pollfd pfd;
		pfd.fd = p->fd[0];
		pfd.events = POLLIN;
		pfd.revents = 0;
		poll(&pfd, 1, 1000);
	}
	plock(p);
#elif WAITERS
	Pwaiter w;
	memset(&w, 0, sizeof w);
	pthread_cond_init(&w.cond, nil);
	w.next = p->waiter;
	p->waiter = &w;
	while(!w.awake)
		pthread_cond_wait(&w.cond, &p->mutex);
	pthread_cond_destroy(&w.cond);
#else
	pthread_cond_wait(&p->cond, &p->mutex);
#endif
}

void
pwakeup(Psleep *p)
{
#if PIPES
	char c = 0;
	int nbad = 0;
	if(p->nwrite < p->nread){
		p->nwrite++;
		while(write(p->fd[1], &c, 1) < 1){
			if(++nbad%100 == 0)
				iprint("pwakeup: write keeps failing\n");
		}
	}
#elif WAITERS
	Pwaiter *w;

	w = p->waiter;
	if(w){
		p->waiter = w->next;
		w->awake = 1;
		pthread_cond_signal(&w->cond);
	}
#else
	pthread_cond_signal(&p->cond);
#endif
}


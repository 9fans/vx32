/*
 * Plan 9 VX timers.  
 *
 * In Plan 9 VX, "ticks" are milliseconds,
 * and "fast ticks" are nanoseconds.
 * This makes the conversions trivial.
 */

#include "u.h"
#include <pthread.h>
#include <signal.h>
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "error.h"
#include "ureg.h"

#define nsec() fastticks(nil)
#define ns2fastticks(x) (x)

struct Timers
{
	Lock lk;
	Timer	*head;
};

static vlong start;
static Timers timers;
static void kicktimerproc(void);

static vlong
tadd(Timers *tt, Timer *nt)
{
	Timer *t, **last;

	/* Called with tt locked */
	assert(nt->tt == nil);
	switch(nt->tmode){
	default:
		panic("timer");
		break;
	case Trelative:
		if(nt->tns <= 0)
			nt->tns = 1;
		nt->twhen = fastticks(nil) + ns2fastticks(nt->tns);
		break;
	case Tperiodic:
		assert(nt->tns >= 100000);	/* At least 100 µs period */
		if(nt->twhen == 0){
			/* look for another timer at same frequency for combining */
			for(t = tt->head; t; t = t->tnext){
				if(t->tmode == Tperiodic && t->tns == nt->tns)
					break;
			}
			if (t)
				nt->twhen = t->twhen;
			else
				nt->twhen = fastticks(nil);
		}
		nt->twhen += ns2fastticks(nt->tns);
		break;
	}

	for(last = &tt->head; (t = *last); last = &t->tnext){
		if(t->twhen > nt->twhen)
			break;
	}
	nt->tnext = *last;
	*last = nt;
	nt->tt = tt;
	if(last == &tt->head)
		return nt->twhen;
	return 0;
}

static uvlong
tdel(Timer *dt)
{
	Timer *t, **last;
	Timers *tt;

	tt = dt->tt;
	if (tt == nil)
		return 0;
	for(last = &tt->head; (t = *last); last = &t->tnext){
		if(t == dt){
			assert(dt->tt);
			dt->tt = nil;
			*last = t->tnext;
			break;
		}
	}
	if(last == &tt->head && tt->head)
		return tt->head->twhen;
	return 0;
}

/* add or modify a timer */
void
timeradd(Timer *nt)
{
	Timers *tt;
	vlong when;

	/* Must lock Timer struct before Timers struct */
	ilock(&nt->lk);
	if((tt = nt->tt)){
		ilock(&tt->lk);
		tdel(nt);
		iunlock(&tt->lk);
	}
	tt = &timers;
	ilock(&tt->lk);
	when = tadd(tt, nt);
	if(when)
		kicktimerproc();
	iunlock(&tt->lk);
	iunlock(&nt->lk);
}

void
timerdel(Timer *dt)
{
	Timers *tt;
	uvlong when;

	ilock(&dt->lk);
	if((tt = dt->tt)){
		ilock(&tt->lk);
		when = tdel(dt);
		if(when && tt == &timers)
			kicktimerproc();
		iunlock(&tt->lk);
	}
	iunlock(&dt->lk);
}

/*
 * The timer proc sleeps until the next timer is going
 * to go off, and then runs any timers that need running.
 * If a new timer is inserted while it is asleep, the proc
 * that adds the new timer will send the timer proc a SIGURG
 * to wake it up early.  SIGURG is blocked in all procs by default,
 * so the timer is guaranteed to get the signal.
 */
static pthread_t timer_pid;
void
timerkproc(void *v)
{
	sigset_t sigs;
	Timers *tt;
	Timer *t;
	uvlong when, now;
	struct timespec ts;
	Ureg u;
	int signo;
	
	memset(&u, 0, sizeof u);
	timer_pid = pthread_self();
	
	tt = &timers;
	ilock(&tt->lk);
	for(;;){
		if((t = tt->head) == nil){
			iunlock(&tt->lk);
			sigemptyset(&sigs);
			sigaddset(&sigs, SIGURG);
			sigwait(&sigs, &signo);
			ilock(&tt->lk);
			continue;
		}
		/*
		 * No need to ilock t here: any manipulation of t
		 * requires tdel(t) and this must be done with a
		 * lock to tt held.  We have tt, so the tdel will
		 * wait until we're done
		 */
		now = fastticks(nil);
		when = t->twhen;
		if(when > now){
			iunlock(&tt->lk);
			when -= now;
			ts.tv_sec = when/1000000000;
			ts.tv_nsec = when%1000000000;
			pthread_sigmask(SIG_SETMASK, nil, &sigs);
			sigdelset(&sigs, SIGURG);
			pselect(0, nil, nil, nil, &ts, &sigs);
			ilock(&tt->lk);
			continue;
		}
		
		tt->head = t->tnext;
		assert(t->tt == tt);
		t->tt = nil;
		iunlock(&tt->lk);
		(*t->tf)(&u, t);
		ilock(&tt->lk);
		if(t->tmode == Tperiodic)
			tadd(tt, t);
	}			
}

static void
kicktimerproc(void)
{
	if(timer_pid != 0)
		pthread_kill(timer_pid, SIGURG);
}

void
timersinit(void)
{
	kproc("*timer*", timerkproc, nil);
}

static Alarms	alarms;
static Timer	alarmtimer;

static void setalarm(void);

static void
soundalarms(Ureg *u, Timer *t)
{
	ulong now;
	Proc *rp;
	
	now = msec();
	qlock(&alarms.lk);
	while((rp = alarms.head) && rp->alarm <= now){
		if(rp->alarm != 0){
			if(!canqlock(&rp->debug))
				break;
			if(!waserror()){
				postnote(rp, 0, "alarm", NUser);
				poperror();
			}
			qunlock(&rp->debug);
			rp->alarm = 0;
		}
		alarms.head = rp->palarm;
	}
	setalarm();
	qunlock(&alarms.lk);		
}

static void
setalarm(void)
{
	Proc *p;
	ulong now;
	
	now = msec();
	if(alarmtimer.tt)
		timerdel(&alarmtimer);
	while((p = alarms.head) && p->alarm == 0)
		alarms.head = p->palarm;
	if(p == nil)
		return;
	alarmtimer.tf = soundalarms;
	alarmtimer.tmode = Trelative;
	alarmtimer.tns = (p->alarm - now)*1000000LL;
	timeradd(&alarmtimer);
}

ulong
procalarm(ulong time)
{
	Proc **l, *f;
	ulong when, old, now;

	now = msec();
	if(up->alarm)
		old = up->alarm - now;
	else
		old = 0;
	if(time == 0) {
		up->alarm = 0;
		return old;
	}
	when = time+now;

	qlock(&alarms.lk);
	l = &alarms.head;
	for(f = *l; f; f = f->palarm) {
		if(up == f){
			*l = f->palarm;
			break;
		}
		l = &f->palarm;
	}

	up->palarm = 0;
	if(alarms.head) {
		l = &alarms.head;
		for(f = *l; f; f = f->palarm) {
			if(f->alarm > when) {
				up->palarm = f;
				*l = up;
				goto done;
			}
			l = &f->palarm;
		}
		*l = up;
	}
	else
		alarms.head = up;
done:
	up->alarm = when;
	setalarm();
	qunlock(&alarms.lk);

	return old;
}

ulong
perfticks(void)
{
	return msec();
}

// Only gets used by the profiler.
// Not going to bother for now.
Timer*
addclock0link(void (*x)(void), int y)
{
	return 0;
}

uvlong
fastticks(uvlong *hz)
{
	struct timeval tv;
	uvlong t;
	
	gettimeofday(&tv, 0);
	t = tv.tv_sec * 1000000000LL + tv.tv_usec*1000LL;
	if(hz)
		*hz = 1000000000LL;
	return t;
}

ulong
msec(void)
{
	struct timeval tv;
	
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000 + tv.tv_usec/1000;
}

ulong
tk2ms(ulong x)
{
	return x;
}

ulong
ms2tk(ulong x)
{
	return x;
}

long
seconds(void)
{
	return time(0);
}

void
todinit(void)
{
	start = todget(nil);
}

void
pcycles(uvlong *t)
{
	*t = fastticks(nil);
}

void (*cycles)(uvlong*) = pcycles;

void
microdelay(int x)
{
	struct timeval tv;
	
	tv.tv_sec = x/1000000;
	tv.tv_usec = x%1000000;
	select(0, nil, nil, nil, &tv);
}

/*
 * Time of day
 */
vlong
todget(vlong *ticksp)
{
	struct timeval tv;
	vlong t;
	gettimeofday(&tv, NULL);
	t = tv.tv_sec*1000000000LL + tv.tv_usec*1000LL;
	if(ticksp)
		*ticksp = t - start;
	return t;
}

void
todset(vlong a, vlong b, int c)
{
	USED(a);
	USED(b);
	USED(c);
}

void
todsetfreq(vlong a)
{
	USED(a);
}


#define	WANT_M
#include	"u.h"
#include	"tos.h"
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"error.h"

#include	"a.out.h"

int	shargs(char*, int, char**);

extern void checkpages(void);
extern void checkpagerefs(void);

long
sysr1(uint32 *x)
{
	vx32sysr1();
	return 0;
}

long
sysrfork(uint32 *arg)
{
	Proc *p;
	int n, i;
	Fgrp *ofg;
	Pgrp *opg;
	Rgrp *org;
	Egrp *oeg;
	ulong pid, flag;
	Mach *wm;

	flag = arg[0];
	/* Check flags before we commit */
	if((flag & (RFFDG|RFCFDG)) == (RFFDG|RFCFDG))
		error(Ebadarg);
	if((flag & (RFNAMEG|RFCNAMEG)) == (RFNAMEG|RFCNAMEG))
		error(Ebadarg);
	if((flag & (RFENVG|RFCENVG)) == (RFENVG|RFCENVG))
		error(Ebadarg);

	if((flag&RFPROC) == 0) {
		if(flag & (RFMEM|RFNOWAIT))
			error(Ebadarg);
		if(flag & (RFFDG|RFCFDG)) {
			ofg = up->fgrp;
			if(flag & RFFDG)
				up->fgrp = dupfgrp(ofg);
			else
				up->fgrp = dupfgrp(nil);
			closefgrp(ofg);
		}
		if(flag & (RFNAMEG|RFCNAMEG)) {
			opg = up->pgrp;
			up->pgrp = newpgrp();
			if(flag & RFNAMEG)
				pgrpcpy(up->pgrp, opg);
			/* inherit noattach */
			up->pgrp->noattach = opg->noattach;
			closepgrp(opg);
		}
		if(flag & RFNOMNT)
			up->pgrp->noattach = 1;
		if(flag & RFREND) {
			org = up->rgrp;
			up->rgrp = newrgrp();
			closergrp(org);
		}
		if(flag & (RFENVG|RFCENVG)) {
			oeg = up->egrp;
			up->egrp = smalloc(sizeof(Egrp));
			up->egrp->ref.ref = 1;
			if(flag & RFENVG)
				envcpy(up->egrp, oeg);
			closeegrp(oeg);
		}
		if(flag & RFNOTEG)
			up->noteid = incref(&noteidalloc);
		return 0;
	}

	p = newproc();

	p->fpsave = up->fpsave;
	p->scallnr = up->scallnr;
	p->s = up->s;
	p->nerrlab = 0;
	p->slash = up->slash;
	p->dot = up->dot;
	incref(&p->dot->ref);

	memmove(p->note, up->note, sizeof(p->note));
	p->privatemem = up->privatemem;
	p->noswap = up->noswap;
	p->nnote = up->nnote;
	p->notified = 0;
	p->lastnote = up->lastnote;
	p->notify = up->notify;
	p->ureg = up->ureg;
	p->dbgreg = 0;

	/* Make a new set of memory segments */
	n = flag & RFMEM;
	qlock(&p->seglock);
	if(waserror()){
		qunlock(&p->seglock);
		nexterror();
	}
	for(i = 0; i < NSEG; i++)
		if(up->seg[i])
			p->seg[i] = dupseg(up->seg, i, n);
	qunlock(&p->seglock);
	poperror();

	/* File descriptors */
	if(flag & (RFFDG|RFCFDG)) {
		if(flag & RFFDG)
			p->fgrp = dupfgrp(up->fgrp);
		else
			p->fgrp = dupfgrp(nil);
	}
	else {
		p->fgrp = up->fgrp;
		incref(&p->fgrp->ref);
	}

	/* Process groups */
	if(flag & (RFNAMEG|RFCNAMEG)) {
		p->pgrp = newpgrp();
		if(flag & RFNAMEG)
			pgrpcpy(p->pgrp, up->pgrp);
		/* inherit noattach */
		p->pgrp->noattach = up->pgrp->noattach;
	}
	else {
		p->pgrp = up->pgrp;
		incref(&p->pgrp->ref);
	}
	if(flag & RFNOMNT)
		up->pgrp->noattach = 1;

	if(flag & RFREND)
		p->rgrp = newrgrp();
	else {
		incref(&up->rgrp->ref);
		p->rgrp = up->rgrp;
	}

	/* Environment group */
	if(flag & (RFENVG|RFCENVG)) {
		p->egrp = smalloc(sizeof(Egrp));
		p->egrp->ref.ref = 1;
		if(flag & RFENVG)
			envcpy(p->egrp, up->egrp);
	}
	else {
		p->egrp = up->egrp;
		incref(&p->egrp->ref);
	}
	p->hang = up->hang;
	p->procmode = up->procmode;

	/* Craft a return frame which will cause the child to pop out of
	 * the scheduler in user mode with the return register zero
	 */
	forkchild(p, up->dbgreg);

	p->parent = up;
	p->parentpid = up->pid;
	if(flag&RFNOWAIT)
		p->parentpid = 0;
	else {
		lock(&up->exl);
		up->nchild++;
		unlock(&up->exl);
	}
	if((flag&RFNOTEG) == 0)
		p->noteid = up->noteid;

	p->fpstate = up->fpstate;
	pid = p->pid;
	memset(p->time, 0, sizeof(p->time));
	p->time[TReal] = msec();

	kstrdup(&p->text, up->text);
	kstrdup(&p->user, up->user);
	/*
	 *  since the bss/data segments are now shareable,
	 *  any mmu info about this process is now stale
	 *  (i.e. has bad properties) and has to be discarded.
	 */
	flushmmu();
	p->basepri = up->basepri;
	p->priority = up->basepri;
	p->fixedpri = up->fixedpri;
	p->mp = up->mp;
	wm = up->wired;
	if(wm)
		procwired(p, wm->machno);
	ready(p);
	sched();
	return pid;
}

static uint32
l2be(uint32 l)
{
	uchar *cp;

	cp = (uchar*)&l;
	return (cp[0]<<24) | (cp[1]<<16) | (cp[2]<<8) | cp[3];
}

static char Echanged[] = "exec arguments changed underfoot";

long
sysexec(uint32 *arg)
{
	char *volatile elem, *volatile file, *ufile;
	Chan *volatile tc;

	/*
	 * Open the file, remembering the final element and the full name.
	 */
	file = nil;
	elem = nil;
	tc = nil;
	if(waserror()){
		if(file)
			free(file);
		if(elem)
			free(elem);
		if(tc)
			cclose(tc);
		nexterror();
	}

	ufile = uvalidaddr(arg[0], 1, 0);
	file = validnamedup(ufile, 1);
	tc = namec(file, Aopen, OEXEC, 0);
	kstrdup((char**)&elem, up->genbuf);

	/*
	 * Read the header.  If it's a #!, fill in progarg[] with info and repeat.
	 */
	int i, n, nprogarg;
	char *progarg[sizeof(Exec)/2+1];
	char *prog, *p;
	char line[sizeof(Exec)+1];
	Exec exec;

	nprogarg = 0;
	n = devtab[tc->type]->read(tc, &exec, sizeof(Exec), 0);
	if(n < 2)
		error(Ebadexec);
	p = (char*)&exec;
	if(p[0] == '#' && p[1] == '!'){
		memmove(line, p, n);
		nprogarg = shargs(line, n, progarg);
		if(nprogarg == 0)
			error(Ebadexec);
		
		/* The original file becomes an extra arg after #! line */
		progarg[nprogarg++] = file;
		
		/*
		 * Take the #! $0 as a file to open, and replace
		 * $0 with the original path's name.
		 */
		prog = progarg[0];
		progarg[0] = elem;
		cclose(tc);
		tc = nil;	/* in case namec errors out */
		tc = namec(prog, Aopen, OEXEC, 0);
		n = devtab[tc->type]->read(tc, &exec, sizeof(Exec), 0);
		if(n < 2)
			error(Ebadexec);
	}

	/* 
	 * #! has had its chance, now we need a real binary
	 */
	uint32 magic, entry, text, etext, data, edata, bss, ebss;

	magic = l2be(exec.magic);
	if(n != sizeof(Exec) || l2be(exec.magic) != AOUT_MAGIC)
		error(Ebadexec);

	entry = l2be(exec.entry);
	text = l2be(exec.text);
	data = l2be(exec.data);
	bss = l2be(exec.bss);
	etext = ROUND(UTZERO+sizeof(Exec)+text, BY2PG);
	edata = ROUND(etext + data, BY2PG);
	ebss = ROUND(etext + data + bss, BY2PG);
	
//iprint("entry %#lux text %#lux data %#lux bss %#lux\n", entry, text, data, bss);
//iprint("etext %#lux edata %#lux ebss %#lux\n", etext, edata, ebss);

	if(entry < UTZERO+sizeof(Exec) || entry >= UTZERO+sizeof(Exec)+text)
		error(Ebadexec);
	
	/* many overflow possibilities */
	if(text >= USTKTOP || data >= USTKTOP || bss >= USTKTOP
	|| etext >= USTKTOP || edata >= USTKTOP || ebss >= USTKTOP
	|| etext >= USTKTOP || edata < etext || ebss < edata)
		error(Ebadexec);

	/*
	 * Copy argv into new stack segment temporarily mapped elsewhere.
	 * Be careful: multithreaded program could be changing argv during this.
	 * Pass 1: count number of arguments, string bytes.
	 */
	int nargv, strbytes;
	uint32 argp, ssize, spage;

	strbytes = 0;
	for(i=0; i<nprogarg; i++)
		strbytes += strlen(progarg[i]) + 1;

	argp = arg[1];
	for(nargv=0;; nargv++, argp += BY2WD){
		uint32 a;
		char *str;

		a = *(uint32*)uvalidaddr(argp, BY2WD, 0);
		if(a == 0)
			break;
		str = uvalidaddr(a, 1, 0);
		n = ((char*)vmemchr(str, 0, 0x7FFFFFFF) - str) + 1;
		if(nprogarg > 0 && nargv == 0)
			continue;	/* going to skip argv[0] on #! */
		strbytes += n;
	}
	if(nargv == 0)
		error("exec missing argv");

	/* 
	 * Skip over argv[0] if using #!.  Waited until now so that
	 * string would still be checked for validity during loop.
	 */
	if(nprogarg > 0){
		nargv--;
		arg[1] += BY2WD;
	}

	ssize = BY2WD*((nprogarg+nargv)+1) + ROUND(strbytes, BY2WD) + sizeof(Tos);

	/*
	 * 8-byte align SP for those (e.g. sparc) that need it.
	 * execregs() will subtract another 4 bytes for argc.
	 */
	if((ssize+4) & 7)
		ssize += 4;
	spage = (ssize+(BY2PG-1)) >> PGSHIFT;

	/*
	 * Pass 2: build the stack segment, being careful not to assume
	 * that the counts from pass 1 are still valid.
	 */
	if(spage > TSTKSIZ)
		error(Enovmem);

	qlock(&up->seglock);
	if(waserror()){
		if(up->seg[ESEG]){
			putseg(up->seg[ESEG]);
			up->seg[ESEG] = nil;
		}
		qunlock(&up->seglock);
		nexterror();
	}
	up->seg[ESEG] = newseg(SG_STACK, TSTKTOP-USTKSIZE, USTKSIZE/BY2PG);
	flushmmu();	// Needed for Plan 9 VX  XXX really?

	/*
	 * Top-of-stack structure.
	 */
	uchar *uzero;
	uzero = up->pmmu.uzero;
	Tos *tos;
	uint32 utos;
	utos = USTKTOP - sizeof(Tos);
	tos = (Tos*)(uzero + utos + TSTKTOP - USTKTOP);
	tos->cyclefreq = m->cyclefreq;
	cycles((uvlong*)&tos->pcycles);
	tos->pcycles = -tos->pcycles;
	tos->kcycles = tos->pcycles;
	tos->clock = 0;

	/*
	 * Argument pointers and strings, together.
	 */
	char *bp, *ep;
	uint32 *targp;
	uint32 ustrp, uargp;

	ustrp = utos - ROUND(strbytes, BY2WD);
	uargp = ustrp - BY2WD*((nprogarg+nargv)+1);
	bp = (char*)(uzero + ustrp + TSTKTOP - USTKTOP);
	ep = bp + strbytes;
	p = bp;
	targp = (uint32*)(uzero + uargp + TSTKTOP - USTKTOP);
	
	/* #! args are trusted */
	for(i=0; i<nprogarg; i++){
		n = strlen(progarg[i]) + 1;
		if(n  > ep - p)
			error(Echanged);
		memmove(p, progarg[i], n);
		p += n;
		*targp++ = ustrp;
		ustrp += n;
	}
	
	/* the rest are not */
	argp = arg[1];
	for(i=0; i<nargv; i++){
		uint32 a;
		char *str;
		
		a = *(uint32*)uvalidaddr(argp, BY2WD, 0);
		argp += BY2WD;
		
		str = uvalidaddr(a, 1, 0);
		n = ((char*)vmemchr(str, 0, 0x7FFFFFFF) - str) + 1;
		if(n  > ep - p)
			error(Echanged);
		memmove(p, str, n);
		p += n;
		*targp++ = ustrp;
		ustrp += n;
	}

	if(*(uint32*)uvalidaddr(argp, BY2WD, 0) != 0)
		error(Echanged);	
	*targp = 0;

	/*
	 * But wait, there's more: prepare an arg copy for up->args
	 * using the copy we just made in the temporary segment.
	 */
	char *args;
	int nargs;

	n = p - bp;	/* includes NUL on last arg, so must be > 0 */
	if(n <= 0)	/* nprogarg+nargv > 0; checked above */
		error(Egreg);
	if(n > 128)
		n = 128;
	args = smalloc(n);
	if(waserror()){
		free(args);
		nexterror();
	}
	memmove(args, bp, n);
	/* find beginning of UTF character boundary to place final NUL */
	while(n > 0 && (args[n-1]&0xC0) == 0x80)
		n--;
	args[n-1] = '\0';
	nargs = n;

	/*
	 * Now we're ready to commit.
	 */
	free(up->text);
	up->text = elem;
	free(up->args);
	up->args = args;
	up->nargs = n;
	elem = nil;
	poperror();	/* args */

	/*
	 * Free old memory.  Special segments maintained across exec.
	 */
	Segment *s;
	for(i = SSEG; i <= BSEG; i++) {
		putseg(up->seg[i]);
		up->seg[i] = nil;	/* in case of error */
	}
	for(i = BSEG+1; i< NSEG; i++) {
		s = up->seg[i];
		if(s && (s->type&SG_CEXEC)) {
			putseg(s);
			up->seg[i] = nil;
		}
	}
	
	/*
	 * Close on exec
	 */
	Fgrp *f;
	f = up->fgrp;
	for(i=0; i<=f->maxfd; i++)
		fdclose(i, CCEXEC);

	/* Text.  Shared. Attaches to cache image if possible */
	/* attachimage returns a locked cache image */
	Image *img;
	Segment *ts;
	img = attachimage(SG_TEXT|SG_RONLY, tc, UTZERO, (etext-UTZERO)>>PGSHIFT);
	ts = img->s;
	up->seg[TSEG] = ts;
	ts->flushme = 1;
	ts->fstart = 0;
	ts->flen = sizeof(Exec)+text;
	unlock(&img->ref.lk);

	/* Data. Shared. */
	s = newseg(SG_DATA, etext, (edata-etext)>>PGSHIFT);
	up->seg[DSEG] = s;

	/* Attached by hand */
	incref(&img->ref);
	s->image = img;
	s->fstart = ts->fstart+ts->flen;
	s->flen = data;

	/* BSS. Zero fill on demand */
	up->seg[BSEG] = newseg(SG_BSS, edata, (ebss-edata)>>PGSHIFT);

	/*
	 * Move the stack
	 */
	s = up->seg[ESEG];
	up->seg[ESEG] = 0;
	up->seg[SSEG] = s;
	qunlock(&up->seglock);
	poperror();	/* seglock */

	s->base = USTKTOP-USTKSIZE;
	s->top = USTKTOP;
	relocateseg(s, USTKTOP-TSTKTOP);

	/*
	 *  '/' processes are higher priority (hack to make /ip more responsive).
	 */
	if(devtab[tc->type]->dc == L'/')
		up->basepri = PriRoot;
	up->priority = up->basepri;
	poperror();	/* tc, elem, file */
	cclose(tc);
	free(file);
	// elem is now up->text

	/*
	 *  At this point, the mmu contains info about the old address
	 *  space and needs to be flushed
	 */
	flushmmu();
	qlock(&up->debug);
	up->nnote = 0;
	up->notify = 0;
	up->notified = 0;
	up->privatemem = 0;
	procsetup(up);
	qunlock(&up->debug);
	if(up->hang)
		up->procctl = Proc_stopme;

	return execregs(entry, USTKTOP - uargp, nprogarg+nargv);
}

int
shargs(char *s, int n, char **ap)
{
	int i;

	s += 2;
	n -= 2;		/* skip #! */
	for(i=0; s[i]!='\n'; i++)
		if(i == n-1)
			return 0;
	s[i] = 0;
	*ap = 0;
	i = 0;
	for(;;) {
		while(*s==' ' || *s=='\t')
			s++;
		if(*s == 0)
			break;
		i++;
		*ap++ = s;
		*ap = 0;
		while(*s && *s!=' ' && *s!='\t')
			s++;
		if(*s == 0)
			break;
		else
			*s++ = 0;
	}
	return i;
}

int
return0(void *v)
{
	return 0;
}

long
syssleep(uint32 *arg)
{

	int n;

	n = arg[0];
	if(n <= 0) {
		yield();
		return 0;
	}
	if(n < TK2MS(1))
		n = TK2MS(1);
	tsleep(&up->sleep, return0, 0, n);
	return 0;
}

long
sysalarm(uint32 *arg)
{
	return procalarm(arg[0]);
}

long
sysexits(uint32 *arg)
{
	char *status;
	char *inval = "invalid exit string";
	char buf[ERRMAX];

	if(arg[0]){
		if(waserror())
			status = inval;
		else{
			status = uvalidaddr(arg[0], 1, 0);
			if(vmemchr(status, 0, ERRMAX) == 0){
				memmove(buf, status, ERRMAX);
				buf[ERRMAX-1] = 0;
				status = buf;
			}
			poperror();
		}

	}else
		status = nil;
	pexit(status, 1);
	return 0;		/* not reached */
}

long
sys_wait(uint32 *arg)
{
	int pid;
	Waitmsg w;
	OWaitmsg *ow;

	if(arg[0] == 0)
		return pwait(nil);

	ow = uvalidaddr(arg[0], sizeof(OWaitmsg), 1);
	evenaddr(arg[0]);
	pid = pwait(&w);
	if(pid >= 0){
		readnum(0, ow->pid, NUMSIZE, w.pid, NUMSIZE);
		readnum(0, ow->time+TUser*NUMSIZE, NUMSIZE, w.time[TUser], NUMSIZE);
		readnum(0, ow->time+TSys*NUMSIZE, NUMSIZE, w.time[TSys], NUMSIZE);
		readnum(0, ow->time+TReal*NUMSIZE, NUMSIZE, w.time[TReal], NUMSIZE);
		strncpy(ow->msg, w.msg, sizeof(ow->msg));
		ow->msg[sizeof(ow->msg)-1] = '\0';
	}
	return pid;
}

long
sysawait(uint32 *arg)
{
	int i;
	int pid;
	Waitmsg w;
	uint32 n;
	char *buf;

	n = arg[1];
	buf = uvalidaddr(arg[0], n, 1);
	pid = pwait(&w);
	if(pid < 0)
		return -1;
	i = snprint(buf, n, "%d %lud %lud %lud %q",
		w.pid,
		w.time[TUser], w.time[TSys], w.time[TReal],
		w.msg);

	return i;
}

void
werrstr(char *fmt, ...)
{
	va_list va;

	if(up == nil)
		return;

	va_start(va, fmt);
	vseprint(up->syserrstr, up->syserrstr+ERRMAX, fmt, va);
	va_end(va);
}

static long
generrstr(uint32 addr, uint nbuf)
{
	char tmp[ERRMAX];
	char *buf;

	if(nbuf == 0)
		error(Ebadarg);
	buf = uvalidaddr(addr, nbuf, 1);
	if(nbuf > sizeof tmp)
		nbuf = sizeof tmp;
	memmove(tmp, buf, nbuf);

	/* make sure it's NUL-terminated */
	tmp[nbuf-1] = '\0';
	memmove(buf, up->syserrstr, nbuf);
	buf[nbuf-1] = '\0';
	memmove(up->syserrstr, tmp, nbuf);
	return 0;
}

long
syserrstr(uint32 *arg)
{
	return generrstr(arg[0], arg[1]);
}

/* compatibility for old binaries */
long
sys_errstr(uint32 *arg)
{
	return generrstr(arg[0], 64);
}

long
sysnotify(uint32 *arg)
{
	if(arg[0] != 0)
		uvalidaddr(arg[0], 1, 0);
	up->notify = arg[0];	/* checked again when used */
	return 0;
}

long
sysnoted(uint32 *arg)
{
	if(arg[0]!=NRSTR && !up->notified)
		error(Egreg);
	return 0;
}

long
syssegbrk(uint32 *arg)
{
	int i;
	uint32 addr;
	Segment *s;

	addr = arg[0];
	for(i = 0; i < NSEG; i++) {
		s = up->seg[i];
		if(s == 0 || addr < s->base || addr >= s->top)
			continue;
		switch(s->type&SG_TYPE) {
		case SG_TEXT:
		case SG_DATA:
		case SG_STACK:
			error(Ebadarg);
		default:
			return ibrk(arg[1], i);
		}
	}

	error(Ebadarg);
	return 0;		/* not reached */
}

long
syssegattach(uint32 *arg)
{
	return segattach(up, arg[0], uvalidaddr(arg[1], 1, 0), arg[2], arg[3]);
}

long
syssegdetach(uint32 *arg)
{
	int i;
	uint32 addr;
	Segment *s;

	qlock(&up->seglock);
	if(waserror()){
		qunlock(&up->seglock);
		nexterror();
	}

	s = 0;
	addr = arg[0];
	for(i = 0; i < NSEG; i++)
		if((s = up->seg[i])) {
			qlock(&s->lk);
			if((addr >= s->base && addr < s->top) ||
			   (s->top == s->base && addr == s->base))
				goto found;
			qunlock(&s->lk);
		}

	error(Ebadarg);

found:
	/*
	 * Check we are not detaching the initial stack segment.
	 */
	if(s == up->seg[SSEG]){
		qunlock(&s->lk);
		error(Ebadarg);
	}
	up->seg[i] = 0;
	qunlock(&s->lk);
	putseg(s);
	qunlock(&up->seglock);
	poperror();

	/* Ensure we flush any entries from the lost segment */
	flushmmu();
	return 0;
}

long
syssegfree(uint32 *arg)
{
	Segment *s;
	uint32 from, to;

	from = arg[0];
	s = seg(up, from, 1);
	if(s == nil)
		error(Ebadarg);
	to = (from + arg[1]) & ~(BY2PG-1);
	from = PGROUND(from);

	if(to > s->top) {
		qunlock(&s->lk);
		error(Ebadarg);
	}

	mfreeseg(s, from, (to - from) / BY2PG);
	qunlock(&s->lk);
	flushmmu();

	return 0;
}

/* For binary compatibility */
long
sysbrk_(uint32 *arg)
{
	return ibrk(arg[0], BSEG);
}

long
sysrendezvous(uint32 *arg)
{
	uintptr tag, val;
	Proc *p, **l;

	tag = arg[0];
	l = &REND(up->rgrp, tag);
	up->rendval = ~(uintptr)0;

	lock(&up->rgrp->ref.lk);
	for(p = *l; p; p = p->rendhash) {
		if(p->rendtag == tag) {
			*l = p->rendhash;
			val = p->rendval;
			p->rendval = arg[1];

			while(p->mach != 0)
				;
			ready(p);
			unlock(&up->rgrp->ref.lk);
			return val;
		}
		l = &p->rendhash;
	}

	/* Going to sleep here */
	up->rendtag = tag;
	up->rendval = arg[1];
	up->rendhash = *l;
	*l = up;
	up->state = Rendezvous;
	unlock(&up->rgrp->ref.lk);

	sched();

	return up->rendval;
}

/*
 * The implementation of semaphores is complicated by needing
 * to avoid rescheduling in syssemrelease, so that it is safe
 * to call from real-time processes.  This means syssemrelease
 * cannot acquire any qlocks, only spin locks.
 * 
 * Semacquire and semrelease must both manipulate the semaphore
 * wait list.  Lock-free linked lists only exist in theory, not
 * in practice, so the wait list is protected by a spin lock.
 * 
 * The semaphore value *addr is stored in user memory, so it
 * cannot be read or written while holding spin locks.
 * 
 * Thus, we can access the list only when holding the lock, and
 * we can access the semaphore only when not holding the lock.
 * This makes things interesting.  Note that sleep's condition function
 * is called while holding two locks - r and up->rlock - so it cannot
 * access the semaphore value either.
 * 
 * An acquirer announces its intention to try for the semaphore
 * by putting a Sema structure onto the wait list and then
 * setting Sema.waiting.  After one last check of semaphore,
 * the acquirer sleeps until Sema.waiting==0.  A releaser of n
 * must wake up n acquirers who have Sema.waiting set.  It does
 * this by clearing Sema.waiting and then calling wakeup.
 * 
 * There are three interesting races here.  
 
 * The first is that in this particular sleep/wakeup usage, a single
 * wakeup can rouse a process from two consecutive sleeps!  
 * The ordering is:
 * 
 * 	(a) set Sema.waiting = 1
 * 	(a) call sleep
 * 	(b) set Sema.waiting = 0
 * 	(a) check Sema.waiting inside sleep, return w/o sleeping
 * 	(a) try for semaphore, fail
 * 	(a) set Sema.waiting = 1
 * 	(a) call sleep
 * 	(b) call wakeup(a)
 * 	(a) wake up again
 * 
 * This is okay - semacquire will just go around the loop
 * again.  It does mean that at the top of the for(;;) loop in
 * semacquire, phore.waiting might already be set to 1.
 * 
 * The second is that a releaser might wake an acquirer who is
 * interrupted before he can acquire the lock.  Since
 * release(n) issues only n wakeup calls -- only n can be used
 * anyway -- if the interrupted process is not going to use his
 * wakeup call he must pass it on to another acquirer.
 * 
 * The third race is similar to the second but more subtle.  An
 * acquirer sets waiting=1 and then does a final canacquire()
 * before going to sleep.  The opposite order would result in
 * missing wakeups that happen between canacquire and
 * waiting=1.  (In fact, the whole point of Sema.waiting is to
 * avoid missing wakeups between canacquire() and sleep().) But
 * there can be spurious wakeups between a successful
 * canacquire() and the following semdequeue().  This wakeup is
 * not useful to the acquirer, since he has already acquired
 * the semaphore.  Like in the previous case, though, the
 * acquirer must pass the wakeup call along.
 * 
 * This is all rather subtle.  The code below has been verified
 * with the spin model /sys/src/9/port/semaphore.p.  The
 * original code anticipated the second race but not the first
 * or third, which were caught only with spin.  The first race
 * is mentioned in /sys/doc/sleep.ps, but I'd forgotten about it.
 * It was lucky that my abstract model of sleep/wakeup still managed
 * to preserve that behavior.
 *
 * I remain slightly concerned about memory coherence
 * outside of locks.  The spin model does not take 
 * queued processor writes into account so we have to
 * think hard.  The only variables accessed outside locks
 * are the semaphore value itself and the boolean flag
 * Sema.waiting.  The value is only accessed with cmpswap,
 * whose job description includes doing the right thing as
 * far as memory coherence across processors.  That leaves
 * Sema.waiting.  To handle it, we call coherence() before each
 * read and after each write.		- rsc
 */

/* Add semaphore p with addr a to list in seg. */
static void
semqueue(Segment *s, long *a, Sema *p)
{
	memset(p, 0, sizeof *p);
	p->addr = a;
	lock(&s->sema.rendez.lk);	/* uses s->sema.Rendez.Lock, but no one else is */
	p->next = &s->sema;
	p->prev = s->sema.prev;
	p->next->prev = p;
	p->prev->next = p;
	unlock(&s->sema.rendez.lk);
}

/* Remove semaphore p from list in seg. */
static void
semdequeue(Segment *s, Sema *p)
{
	lock(&s->sema.rendez.lk);
	p->next->prev = p->prev;
	p->prev->next = p->next;
	unlock(&s->sema.rendez.lk);
}

/* Wake up n waiters with addr a on list in seg. */
static void
semwakeup(Segment *s, long *a, long n)
{
	Sema *p;
	
	lock(&s->sema.rendez.lk);
	for(p=s->sema.next; p!=&s->sema && n>0; p=p->next){
		if(p->addr == a && p->waiting){
			p->waiting = 0;
			coherence();
			wakeup(&p->rendez);
			n--;
		}
	}
	unlock(&s->sema.rendez.lk);
}

/* Add delta to semaphore and wake up waiters as appropriate. */
static long
semrelease(Segment *s, long *addr, long delta)
{
	long value;

	do
		value = *addr;
	while(!cmpswap(addr, value, value+delta));
	semwakeup(s, addr, delta);
	return value+delta;
}

/* Try to acquire semaphore using compare-and-swap */
static int
canacquire(long *addr)
{
	long value;
	
	while((value=*addr) > 0)
		if(cmpswap(addr, value, value-1))
			return 1;
	return 0;
}		

/* Should we wake up? */
static int
semawoke(void *p)
{
	coherence();
	return !((Sema*)p)->waiting;
}

/* Acquire semaphore (subtract 1). */
static int
semacquire(Segment *s, long *addr, int block)
{
	int acquired;
	Sema phore;

	if(canacquire(addr))
		return 1;
	if(!block)
		return 0;

	acquired = 0;
	semqueue(s, addr, &phore);
	for(;;){
		phore.waiting = 1;
		coherence();
		if(canacquire(addr)){
			acquired = 1;
			break;
		}
		if(waserror())
			break;
		sleep(&phore.rendez, semawoke, &phore);
		poperror();
	}
	semdequeue(s, &phore);
	coherence();	/* not strictly necessary due to lock in semdequeue */
	if(!phore.waiting)
		semwakeup(s, addr, 1);
	if(!acquired)
		nexterror();
	return 1;
}

long
syssemacquire(uint32 *arg)
{
	int block;
	long *addr;
	Segment *s;

	addr = uvalidaddr(arg[0], sizeof(long), 1);
	evenaddr(arg[0]);
	block = arg[1];
	
	if((s = seg(up, arg[0], 0)) == nil)
		error(Ebadarg);
	if(*addr < 0)
		error(Ebadarg);
	return semacquire(s, addr, block);
}

long
syssemrelease(uint32 *arg)
{
	long *addr, delta;
	Segment *s;

	addr = uvalidaddr(arg[0], sizeof(long), 1);
	evenaddr(arg[0]);
	delta = arg[1];

	if((s = seg(up, arg[0], 0)) == nil)
		error(Ebadarg);
	if(delta < 0 || *addr < 0)
		error(Ebadarg);
	return semrelease(s, addr, arg[1]);
}

long
sysnsec(uint32 *arg)
{
	long *addr;

	addr = uvalidaddr(arg[0], sizeof(vlong), 1);
	evenaddr(arg[0]);

	*(vlong*)addr = todget(nil);

	return 0;
}

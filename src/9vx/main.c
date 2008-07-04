/*
 * Plan 9 VX
 */

#define	WANT_M

#ifdef __APPLE__
#define __DARWIN_UNIX03 0
#endif

#include	"u.h"
#include	"libvx32/vx32.h"
#include	<sys/mman.h>
#include	<signal.h>
#include	<pwd.h>
#include	<pthread.h>
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"ureg.h"
#include	"init.h"
#include	"error.h"
#include	"arg.h"
#include	"tos.h"

#define Image IMAGE
#include	"draw.h"
#include	"memdraw.h"
#include	"cursor.h"
#include	"screen.h"

extern Dev ipdevtab;
extern Dev drawdevtab;
extern Dev fsdevtab;
extern Dev audiodevtab;

int	doabort = 1;	// for now
char*	argv0;
char*	conffile = "9vx";
Conf	conf;

static int	bootboot;	/* run /boot/boot instead of bootscript */
static int	initrc;	/* run rc instead of init */
static char*	username;
static Mach mach0;

extern char*	localroot;
extern int	tracemmu;
extern int tracekdev;
extern int nuspace;
static int singlethread;

static void	bootinit(void);
static void	siginit(void);

static char*	getuser(void);
static char*	findroot(void);

void
usage(void)
{
	fprint(2, "usage: 9vx [-gt] [-r root] [-u user]\n");
	exit(1);
}

void
nop(void)
{
}

int
main(int argc, char **argv)
{
	int usetty;
	int nogui;
	int nofork;
	char buf[1024];
	
	/* Minimal set up to make print work. */
	setmach(&mach0);
	coherence = nop;
	quotefmtinstall();
	
	nogui = 0;
	nofork = 0;
	usetty = 0;
	localroot = nil;
	ARGBEGIN{
	/* debugging options */
	case '1':
		singlethread = 1;
		break;
	case 'A':
		doabort++;
		break;
	case 'K':
		tracekdev++;
		break;
	case 'F':
		nofork = 1;
		break;
	case 'M':
		tracemmu++;
		break;
	case 'P':
		traceprocs++;
		break;
	case 'S':
		tracesyscalls++;
		break;
	case 'U':
		nuspace = atoi(EARGF(usage()));
		break;
	case 'X':
		vx32_debugxlate++;
		break;
	
	/* real options */
	case 'b':
		bootboot = 1;
		break;
	case 'g':
		nogui = 1;
		usetty = 1;
		break;
	case 'i':
		initrc = 1;
		break;
	case 'r':
		localroot = EARGF(usage());
		break;
	case 'u':
		username = EARGF(usage());
		break;
	case 't':
		usetty = 1;
		break;
	default:
		usage();
	}ARGEND
	
	if(argc != 0)
		usage();
	
	if(!bootboot){
		if(localroot == nil && (localroot = findroot()) == nil)
			panic("cannot find plan 9 root; use -r");
		snprint(buf, sizeof buf, "%s/386/bin/rc", localroot);
		if(access(buf, 0) < 0)
			panic("%s does not exist", buf);
	}

	if(username == nil && (username = getuser()) == nil)
		username = "tor";
	eve = strdup(username);
	if(eve == nil)
		panic("strdup eve");

	mach0init();
	mmuinit();
	confinit();
	conf.monitor = !nogui;

	/*
	 * Unless we're going to use the terminal for input/output,
	 * fork into the background.  This is a little dicey, since we
	 * haven't allocated the screen yet, but that would kick off
	 * a kproc, and we need to fork before we start any pthreads.
	 * Cannot fork on OS X - it can't handle it.
	 */
#ifndef __APPLE__
	if(!usetty && !nofork && fork() > 0)
		_exit(0);
#endif

	/*
	 * Have to do this after fork; on OS X child does
	 * not inherit sigaltstack.
	 */
	siginit();

	/*
	 * Debugging: tell user what options we guessed.
	 */
	print("9vx %s-r %s -u %s\n", usetty ? "-t " : "", localroot, username);

	printinit();
	procinit0();
	initseg();
	chandevreset();
	if(!singlethread){
		makekprocdev(&ipdevtab);
		makekprocdev(&fsdevtab);
		makekprocdev(&drawdevtab);
		makekprocdev(&audiodevtab);
	}
	bootinit();
	pageinit();
#ifdef __APPLE__
	if(conf.monitor)
		screeninit();
	extern void machsiginit(void);
	machsiginit();
#endif
	userinit();
	terminit(!usetty);
	uartinit(usetty);
	timersinit();
	active.thunderbirdsarego = 1;
	schedinit();
	return 0;  // Not reached
}

/*
 * Search for Plan 9 /386/bin/rc to find root.
 */
static char*
findroot(void)
{
	static char cwd[1024];
	int i;
	char buf[1024];
	char *dir[] = {
		cwd,
		"/Users/rsc/9vx",
		"/home/rsc/plan9/4e"
	};
	
	if(getcwd(cwd, sizeof cwd) == nil){
		oserrstr();
		panic("getcwd: %r");
	}

	for(i=0; i<nelem(dir); i++){
		snprint(buf, sizeof buf, "%s/386/bin/rc", dir[i]);
		if(access(buf, 0) >= 0)
			return dir[i];
	}
	return nil;
}

static char*
getuser(void)
{
	struct passwd *pw;

	pw = getpwuid(getuid());
	if(pw == nil)
		return nil;
	return strdup(pw->pw_name);
}

/*
 * Initial config.
 */
void
confinit(void)
{
	int i;

	conf.npage = 0;
	for(i=0; i<nelem(conf.mem); i++)
		conf.npage += conf.mem[i].npage;
	conf.upages = conf.npage;
	conf.nproc = 100 + ((conf.npage*BY2PG)/MB)*5;
	if(conf.nproc > 2000)
		conf.nproc = 2000;
	conf.nimage = 200;
	conf.nswap = 0;
	conf.nswppo = 0;
	conf.ialloc = 1<<20;
}

/*
 * Simple boot script.  Init0 takes care of setting up the
 * environment variables and name space bindings that
 * are needed to run rc and /386/init. 
 * Can't use #!/386/init -t directly, because init will treat
 * the extra argument /boot/boot as a command to run,
 * which will cause a loop.
 */
static char *bootscript =
	"#!/386/bin/rc\n"
	"exec /386/init -t\n";

static char *rcscript =
	"#!/386/bin/rc\n"
	"/386/bin/rc -i\n";

static void
bootinit(void)
{
	/*
	 * libauth refuses to use anything but /boot/factotum
	 * to ask for keys, so we have to embed a factotum binary,
	 * even if we don't execute it to provide a file system.
	 * Also, maybe /boot/boot needs it.
	 */
	extern uchar factotumcode[];
	extern long factotumlen;

	if(bootboot){
		extern uchar bootcode[];
		extern long bootlen;
		addbootfile("boot", bootcode, bootlen);
	}else if(initrc)
		addbootfile("boot", (uchar*)rcscript, strlen(rcscript));
	else
		addbootfile("boot", (uchar*)bootscript, strlen(bootscript));
	addbootfile("factotum", factotumcode, factotumlen);
}

static uchar *sp;	/* user stack of init proc */
static void init0(void);

/*
 * Set up first process.
 */
void
userinit(void)
{
	void *v;
	Proc *p;
	Segment *s;
	Page *pg;

	p = newproc();
	p->pgrp = newpgrp();
	p->egrp = smalloc(sizeof(Egrp));
	p->egrp->ref.ref = 1;
	p->fgrp = dupfgrp(nil);
	p->rgrp = newrgrp();
	p->procmode = 0640;

	kstrdup(&eve, username);
	kstrdup(&p->text, "*init*");
	kstrdup(&p->user, eve);

	p->fpstate = FPinit;
	fpoff();

	/*
	 * Kernel Stack
	 *
	 * N.B. make sure there's enough space for syscall to check
	 *	for valid args and 
	 *	4 bytes for gotolabel's return PC
	 */
	labelinit(&p->sched, (ulong)init0, (ulong)p->kstack+KSTACK-(sizeof(Sargs)+BY2WD));

	/*
	 * User Stack
	 *
	 * N.B. cannot call newpage() with clear=1, because pc kmap
	 * requires up != nil.  use tmpmap instead.
	 */
	s = newseg(SG_STACK, USTKTOP-USTKSIZE, USTKSIZE/BY2PG);
	p->seg[SSEG] = s;
	pg = newpage(0, 0, USTKTOP-BY2PG);
	v = tmpmap(pg);
	memset(v, 0, BY2PG);
	segpage(s, pg);
	bootargs(v);
	tmpunmap(v);

	/*
	 * Text
	 */
	s = newseg(SG_TEXT, UTZERO, 1);
	s->flushme++;
	p->seg[TSEG] = s;
	pg = newpage(0, 0, UTZERO);
	segpage(s, pg);
	v = tmpmap(pg);
	memset(v, 0, BY2PG);
	memmove(v, initcode, sizeof initcode);
	tmpunmap(v);

	ready(p);
}

static uchar*
pusharg(char *p)
{
	int n;

	n = strlen(p)+1;
	sp -= n;
	memmove(sp, p, n);
	return sp;
}

void
bootargs(void *base)
{
 	int i, ac;
	uchar *av[32];
	uchar **lsp;

	sp = (uchar*)base + BY2PG - MAXSYSARG*BY2WD - sizeof(Tos);

	ac = 0;
	av[ac++] = pusharg("9vx");
	/* TODO: could use command line argc, argv here if it was useful */

	/* 4 byte word align stack */
	sp = (uchar*)((ulong)sp & ~3);

	/* build argc, argv on stack */
	sp -= (ac+2)*sizeof(sp);
	lsp = (uchar**)sp;
	*lsp++ = (uchar*)ac;
	for(i = 0; i < ac; i++)
		*lsp++ = av[i] + ((USTKTOP - BY2PG) - (ulong)base);
	*lsp = 0;
	sp += (USTKTOP - BY2PG) - (ulong)base;
}

void
showexec(ulong sp)
{
	ulong *a, *argv;
	int i, n;
	uchar *uzero;
	
	uzero = up->pmmu.uzero;
	iprint("showexec %p\n", sp);
	if(sp >= USTKTOP || sp < USTKTOP-USTKSIZE)
		panic("showexec: bad sp");
	a = (ulong*)(uzero + sp);
	n = *a++;
	iprint("argc=%d\n", n);
	argv = a;
	iprint("argv=%p\n", argv);
	for(i=0; i<n; i++){
		iprint("argv[%d]=%p\n", i, argv[i]);
		iprint("\t%s\n", (uzero + argv[i]));
	}
	iprint("argv[%d]=%p\n", i, argv[i]);
}


/*
 * First process starts executing, with up set, here.
 */
static void
init0(void)
{
	char buf[2*KNAMELEN];
	Chan *c, *srvc;
	char *root;

	up->nerrlab = 0;
	if(waserror())
		panic("init0: %r");

	spllo();

	/*
	 * These are o.k. because rootinit is null.
	 * Then early kproc's will have a root and dot.
	 */
	up->slash = namec("#/", Atodir, 0, 0);
	pathclose(up->slash->path);
	up->slash->path = newpath("/");
	up->dot = cclone(up->slash);

	chandevinit();

	/* set up environment */
	snprint(buf, sizeof(buf), "%s %s", "vx32" /*arch->id*/, conffile);
	ksetenv("terminal", buf, 0);
	ksetenv("cputype", "386", 0);
	ksetenv("rootdir", "/root", 0);
	if(cpuserver)
		ksetenv("service", "cpu", 0);
	else
		ksetenv("service", "terminal", 0);
	ksetenv("user", username, 0);
	ksetenv("sysname", "vx32", 0);
	
	/* if we're not running /boot/boot, mount / and create /srv/boot */
	if(!bootboot){
		kbind("#Zplan9/", "/", MAFTER);
		kbind("#p", "/proc", MREPL);

		/* create working /srv/boot for backward compatibility */
		c = namec("#~/mntloop", Aopen, ORDWR, 0);
		root = "#Zplan9/";
		devtab[c->type]->write(c, root, strlen(root), 0);
		srvc = namec("#s/boot", Acreate, OWRITE, 0666);
		ksrvadd(srvc, c);
		cclose(srvc);
		cclose(c);
	}

	poperror();

//showexec((ulong)sp);
	touser(sp);	/* infinity, and beyond. */
}

int invx32;	/* shared with sbcl-signal.c */

/*
 * SIGSEGV handler.  If we get SIGSEGV while executing
 * in the kernel on behalf of user code, then we call fault
 * to map in the missing page, just as we would on a MIPS
 * or any other architecture with a software-managed TLB.
 */
void
sigsegv(int signo, siginfo_t *info, void *v)
{
	int read;
	ulong addr, eip, esp;
	ucontext_t *uc;
	uchar *uzero;

	if(m == nil)
		panic("sigsegv: m == nil");
	if(m->machno != 0)
		panic("sigsegv on cpu%d", m->machno);
	if(up == nil)
		panic("sigsegv: up == nil");

	uzero = up->pmmu.uzero;

	uc = v;
#if defined(__APPLE__)
	mcontext_t mc;
	mc = uc->uc_mcontext;
	addr = (ulong)info->si_addr;
	read = !(mc->es.err&2);
	eip = mc->ss.eip;
	esp = mc->ss.esp;
#elif defined(__linux__)
	mcontext_t *mc;
	struct sigcontext *ctx;
	mc = &uc->uc_mcontext;
	ctx = (struct sigcontext*)mc;
	addr = (ulong)info->si_addr;
	read = !(ctx->err&2);
	eip = ctx->eip;
	esp = ctx->esp;
#elif defined(__FreeBSD__)
	mcontext_t *mc;
	mc = &uc->uc_mcontext;
	eip = mc->mc_eip;
	esp = mc->mc_esp;
	addr = (ulong)info->si_addr;
	if(__FreeBSD__ < 7){
		/*
		 * FreeBSD /usr/src/sys/i386/i386/trap.c kludgily reuses
		 * frame->tf_err as somewhere to put the faulting address
		 * (cr2) when calling into the generic signal dispatcher.
		 * Unfortunately, that means that the bit in tf_err that says
		 * whether this is a read or write fault is irretrievably gone.
		 * So we have to figure it out.  Let's assume that if the page
		 * is already mapped in core, it is a write fault.  If not, it is a
		 * read fault.  
		 *
		 * This is apparently fixed in FreeBSD 7, but I don't have any
		 * FreeBSD 7 machines on which to verify this.
		 */
		char vec;
		int r;

		vec = 0;
		r = mincore((void*)addr, 1, &vec);
//iprint("FreeBSD fault [%d]: addr=%p[%p] mincore=%d vec=%#x errno=%d\n", signo, addr, (uchar*)addr-uzero, r, vec, errno);
		if(r < 0 || vec == 0)
			mc->mc_err = 0;	/* read fault */
		else
			mc->mc_err = 2;	/* write fault */
	}
	read = !(mc->mc_err&2);
#else
#	error	"Unknown OS in sigsegv"
#endif

	if(0)
		iprint("cpu%d: %ld %s sigsegv [stack=%p eip=%p esp=%p addr=%p[%p] %d]\n",
			m->machno, up ? up->pid : 0, up ? up->text : nil,
			&signo, eip, esp, addr, (uchar*)addr-uzero, read);

	/*
	 * In vx32?  It better handle it.
	 */
	if(invx32){
		if(vx32_sighandler(signo, info, v))
			return;
		panic("user fault: signo=%d addr=%p [useraddr=%p] read=%d eip=%p esp=%p",
			signo, addr, (uchar*)addr-uzero, read, eip, esp);
	}

	/*
	 * Otherwise, invoke the Plan 9 fault handler.
	 * This means we must have been executing in the "kernel",
	 * not user space code.  If the fault can't be fixed,
	 * we screwed up.
	 */
	if(!isuaddr((uchar*)addr) || fault((uchar*)addr - uzero, read) < 0)
		panic("kernel fault: signo=%d addr=%p[%p] %d", signo, addr, (uchar*)addr-uzero, read);
}

/*
 * No-op handler for SIGUSR1 and SIGURG.
 */
static void
signop(int signo, siginfo_t *info, void *v)
{
}

#ifdef TLS
/* __thread means thread-local */
__thread Mach *m;
__thread Proc *up;
#else
static pthread_key_t machkey;

static void
machkeyinit(void)
{
	if(pthread_key_create(&machkey, nil) < 0)
		panic("pthread_key_create: %s", strerror(errno));
	pthread_setspecific(machkey, machp[0]);
}

Mach*
getmach(void)
{
	return pthread_getspecific(machkey);
}

void
setmach(Mach *xm)
{
	if(pthread_setspecific(machkey, xm) < 0)
		panic("pthread_setspecific: %s", strerror(errno));
}
#endif

Mach *machp[MAXMACH] = { &mach0 };

/*
 * We use one signal handler for SIGSEGV, which can happen
 * both during kernel execution and during vx32 execution,
 * but we only want to run on an alternate stack during the latter.
 * The fact that we the signal handler flags are not per-pthread
 * is one thing that keeps us from running parallel vx32 instances.
 */
void
setsigsegv(int vx32)
{
	int flags;
	struct sigaction act;
	stack_t ss;
	
	// Paranoia: better not be on signal stack.
	// Could disable if this is too expensive.
	if (sigaltstack(NULL, &ss) >= 0){
		if(ss.ss_flags & SS_ONSTACK)
			panic("setsigsegv on signal stack");
		if(vx32 && (ss.ss_flags & SS_DISABLE))
			panic("setsigsegv vx32 without alt stack");
	}

	invx32 = vx32;
	flags = SA_SIGINFO;
	if(vx32)
		flags |= SA_ONSTACK|SA_NODEFER;	/* run on vx32 alternate stack */
	else
		flags |= SA_RESTART|SA_NODEFER;	/* allow recursive faults */
	memset(&act, 0, sizeof act);
	act.sa_sigaction = sigsegv;
	act.sa_flags = flags;
	sigaction(SIGSEGV, &act, nil);
	sigaction(SIGBUS, &act, nil);
}

/*
 * Boot-time config of processor 0.
 */
void
mach0init(void)
{
#ifndef TLS
	machkeyinit();
#endif

	conf.nmach = 1;
	machinit();	/* common per-processor init */
	active.machs = 1;
	active.exiting = 0;
}

static void
siginit(void)
{
	struct sigaction act;

	/* Install vx32signal handlers ... */
	vx32_siginit();
	
	/* ... and then our own */
	setsigsegv(0);

	memset(&act, 0, sizeof act);
	act.sa_sigaction = signop;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR1, &act, nil);
	sigaction(SIGURG, &act, nil);
}

void
machinit(void)
{
	sigset_t sigs;
	
	m->perf.period = 1;	/* devcons divides by this */

	/* Block SIGURG except when in timer kproc */
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGURG);
	pthread_sigmask(SIG_BLOCK, &sigs, nil);

}

void*
squidboy(void *v)
{
//	iprint("Hello Squidboy\n");

	setmach(v);
	machinit();
	lock(&active.lk);
	active.machs |= 1<<m->machno;
	unlock(&active.lk);
	schedinit();	/* never returns */
	return 0;
}

/*
 * Allocate a new processor, just like that.
 */
void
newmach(void)
{
	int i;
	Mach *mm;
	pthread_t pid;

	if(singlethread)
		return;

	i = conf.nmach;
	if(i >= MAXMACH)
		panic("out of processors");
	mm = mallocz(sizeof *mm, 1);
	mm->machno = i;
	mm->new = 1;
	machp[i] = mm;
	conf.nmach++;
	
	if(pthread_create(&pid, 0, squidboy, mm) < 0)
		panic("pthread_create");
}

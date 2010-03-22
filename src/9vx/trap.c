/*
 * Heavily modified copy of /sys/src/9/pc/trap.c
 * A shell of its former self.
 */
#define	WANT_M

#include	"u.h"
#include	"libvx32/vx32.h"
#include	"tos.h"
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"ureg.h"
#include	"error.h"
#include	"trace.h"
#include	"systab.h"

int tracesyscalls;
static void noted(Ureg*, ulong);
static void mathemu(Ureg*, void*);
static void syscall(Ureg*);

/* going to user space */
void
kexit(Ureg *ureg)
{
	uvlong t;
	Tos *tos;

	/* precise time accounting, kernel exit */
	tos = (Tos*)(up->pmmu.uzero+USTKTOP-sizeof(Tos));
	cycles(&t);
	tos->kcycles += t - up->kentry;
	tos->pcycles = up->pcycles;
	tos->pid = up->pid;
}

static char* excname[32] = {
	"divide error",
	"debug exception",
	"nonmaskable interrupt",
	"breakpoint",
	"overflow",
	"bounds check",
	"invalid opcode",
	"coprocessor not available",
	"double fault",
	"coprocessor segment overrun",
	"invalid TSS",
	"segment not present",
	"stack exception",
	"general protection violation",
	"page fault",
	"15 (reserved)",
	"coprocessor error",
	"alignment check",
	"machine check",
	"19 (reserved)",
	"20 (reserved)",
	"21 (reserved)",
	"22 (reserved)",
	"23 (reserved)",
	"24 (reserved)",
	"25 (reserved)",
	"26 (reserved)",
	"27 (reserved)",
	"28 (reserved)",
	"29 (reserved)",
	"30 (reserved)",
	"31 (reserved)",
};

/*
 * Handle a trap.
 */
void
trap(Ureg *ureg)
{
	char buf[ERRMAX];
	int vno;

	vno = ureg->trap;

	switch(vno){
	case VXTRAP_FPOFF:
		mathemu(ureg, nil);
		return;
	
	case VXTRAP_SOFT+0x40:	/* int $0x40 - system call */
		if(tracesyscalls){
			uint32 *sp = (uint32*)(up->pmmu.uzero + ureg->usp);
			iprint("%d [%s] %s %#ux %08ux %08ux %08ux %08ux\n",
				up->pid, up->text,
				sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
		}
		syscall(ureg);
		if(tracesyscalls){
			if(ureg->ax == -1)
				iprint("%d [%s] -> %s\n", up->pid, up->text, up->syserrstr);
			else
				iprint("%d [%s] -> %#ux\n", up->pid, up->text, ureg->ax);
		}
		return;
	
	case VXTRAP_IRQ+VXIRQ_TIMER:
		sched();
		break;
	
	case 3:	// breakpoint
		/* restore pc to instruction that caused the trap */
		ureg->pc--;
		sprint(buf, "sys: breakpoint");
		postnote(up, 1, buf, NDebug);
		break;

	default:
		if(vno < nelem(excname)){
			spllo();
			sprint(buf, "sys: trap: %s", excname[vno]);
			postnote(up, 1, buf, NDebug);
		}else{
			dumpregs(ureg);
			if(vno < nelem(excname))
				panic("%s", excname[vno]);
			panic("unknown trap/intr: %#x", vno);
		}
		break;
	}

	if(up->procctl || up->nnote)
		notify(ureg);
	spllo();	/* no actual effect, just silences prints */
	kexit(ureg);
}

void
dumpregs2(Ureg* ureg)
{
	if(up)
		print("cpu%d: registers for %s %lud\n",
			m->machno, up->text, up->pid);
	else
		print("cpu%d: registers for kernel\n", m->machno);
	print("FLAGS=%luX TRAP=%luX ECODE=%luX PC=%luX",
		ureg->flags, ureg->trap, ureg->ecode, ureg->pc);
	print(" USP=%luX\n", ureg->usp);
	print("  AX %8.8luX  BX %8.8luX  CX %8.8luX  DX %8.8luX\n",
		ureg->ax, ureg->bx, ureg->cx, ureg->dx);
	print("  SI %8.8luX  DI %8.8luX  BP %8.8luX\n",
		ureg->si, ureg->di, ureg->bp);
}

void
dumpregs(Ureg* ureg)
{
	dumpregs2(ureg);

	/*
	 * Processor control registers.
	 * If machine check exception, time stamp counter, page size extensions
	 * or enhanced virtual 8086 mode extensions are supported, there is a
	 * CR4. If there is a CR4 and machine check extensions, read the machine
	 * check address and machine check type registers if RDMSR supported.
	print("  CR0 %8.8lux CR2 %8.8lux CR3 %8.8lux",
		getcr0(), getcr2(), getcr3());
	if(m->cpuiddx & 0x9A){
		vlong mca, mct;
		iprint(" CR4 %8.8lux", getcr4());
		if((m->cpuiddx & 0xA0) == 0xA0){
			rdmsr(0x00, &mca);
			rdmsr(0x01, &mct);
			iprint("\n  MCA %8.8llux MCT %8.8llux", mca, mct);
		}
	}
	print("\n  ur %lux up %lux\n", ureg, up);
	 */
}

static void
syscallprint(Ureg *ureg)
{
	uint32 *sp = (uint32*)(up->pmmu.uzero + ureg->usp);
	int syscallno = ureg->ax;

	/* should never happen -- arguably, this should panic. */
	if (up->syscalltrace)
		free(up->syscalltrace);
	switch(syscallno) {
	case SYSR1:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case _ERRSTR:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1]);
	break;
	case BIND:{
		char *s1 =  uvalidaddr(sp[1], 1, 0);
		char *s2 =  uvalidaddr(sp[2], 1, 0);
	up->syscalltrace = smprint("%d [%s] %s %#ux %08x=%s  %08x=%s flags %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], s1, sp[2], s2, sp[3]);
	break;
	}
	case CHDIR:{
		char *s =  uvalidaddr(sp[1], 1, 0);
	up->syscalltrace = smprint("%d [%s] %s %#ux %08x=%s  ",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], s);
	break;
	}
	case CLOSE:
	up->syscalltrace = smprint("%d [%s] %s %#ux %d",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1]);
	break;
	case DUP:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2]);
	break;
	case ALARM:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux ms",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1]);
	break;
	case EXEC: {
		char *name =uvalidaddr(sp[1], 1, 0);
		uint32 *argv = uvalidaddr(sp[2], 1, 0);
		int j = 0, i;
		up->syscalltrace = mallocz(4096,1);
		j += snprint(up->syscalltrace, 4096, "%d [%s] %s %#ux %08ux=%s %08ux=(",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], name, sp[2]);
		/* more than 4096 of args, we just don't do */
		for(i = 0; argv[i] && (j > 0); i++) {
			char *str = uvalidaddr(argv[i], 1, 0);
			j += snprint(up->syscalltrace+j,4096-j, "[%d]=%s ", i, str);
		}
		snprint(up->syscalltrace+j,4096-j, ")");
	break;
	}
	case EXITS:{
		char *msg = sp[1] ? uvalidaddr(sp[1], 1, 0) : "NULL";
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux=%s",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], msg);
	break;
	}
	case _FSESSION:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case FAUTH:{
		char *aname = uvalidaddr(sp[2], 1, 0);
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux=%s",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], aname);
	break;
	}
	case _FSTAT:{
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %#ux %08ux",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	}
	case SEGBRK:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1]);
	break;
	case _MOUNT:{
		char *old =uvalidaddr(sp[3], 1, 0);
		//char *aname = uvalidaddr(sp[5], 1, 0);
	up->syscalltrace = smprint("%d [%s] %s %#ux %d %d %08ux=%s %08ux %#ux",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3], old, sp[4],sp[5]);
	break;
	}
	case OPEN: {
		char *s;
		s = uvalidaddr(sp[1], 1, 0);
		up->syscalltrace = smprint("%d [%s] %s %#ux %08x=%s %08ux",
			up->pid, up->text,
			sysctab[ureg->ax], sp[0], sp[1], s, sp[2]);
	break;
	}
	case _READ: {
	up->syscalltrace = smprint("%d [%s] %s %#ux %d %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	}
	case OSEEK:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case SLEEP: {
	up->syscalltrace = smprint("%d [%s] %s %#ux %d",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1]);
	break;
	}
	case _STAT:{
		char *name = uvalidaddr(sp[1], 1, 0);
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux=%s %#ux %d",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], name, sp[2], sp[3]);
	break;
	}
	case RFORK:{
		uint32 flags = sp[1];
	up->syscalltrace = smprint("%d [%s] %s%s %#ux %08ux",
		up->pid, up->text,
		flags & RFPROC ? "Proc" : "",sysctab[ureg->ax], sp[0], sp[1] );
	break;
	}
	case _WRITE:
	up->syscalltrace = smprint("%d [%s] %s %#ux %d %#ux %d %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case PIPE: 
	up->syscalltrace = smprint("%d [%s] %s %#ux",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0]);
	break;
	case CREATE:{
		char *name = uvalidaddr(sp[1], 1, 0);
	up->syscalltrace = smprint("%d [%s] %s %#ux %#ux=%s %08ux %08ux",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], name, sp[2], sp[3]);
	break;
	}
	case FD2PATH:
	up->syscalltrace = smprint("%d [%s] %s %#ux %d",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1]);
	break;
	case BRK_:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case REMOVE:{
		char *name = uvalidaddr(sp[1], 1, 0);
	up->syscalltrace = smprint("%d [%s] %s %#ux %#ux=%s",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], name);
	break;
	}
	case _WSTAT:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case _FWSTAT:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case NOTIFY:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case NOTED:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case SEGATTACH:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case SEGDETACH:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case SEGFREE:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case SEGFLUSH:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case RENDEZVOUS:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case UNMOUNT:{
		char *name = uvalidaddr(sp[1], 1, 0);
	up->syscalltrace = smprint("%d [%s] %s %#ux %#ux=%s",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], name);
	break;
	}
	case _WAIT:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case SEMACQUIRE:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case SEMRELEASE:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case SEEK:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case FVERSION:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case ERRSTR:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case STAT:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case FSTAT:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case WSTAT:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case FWSTAT:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case MOUNT:{
		char *old =uvalidaddr(sp[3], 1, 0);
		//char *aname = uvalidaddr(sp[5], 1, 0);
	up->syscalltrace = smprint("%d [%s] %s %#ux %d %d %08ux=%s %08ux %#ux",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3], old, sp[4],sp[5]);
	break;
	}
	case AWAIT:
	up->syscalltrace = smprint("%d [%s] %s %#ux %08ux %08ux %08ux %08uxh",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	case PREAD:{
	up->syscalltrace = smprint("%d [%s] %s %#ux %d %#ux %d",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], sp[3]);
	break;
	}
	case PWRITE:{
		int len = sp[3] > 64 ? 64 : sp[3];
  		char *s = uvalidaddr(sp[2], len, 0);
		int i;
		char a[65];
		memset(a, 0, sizeof(a));
		for(i = 0; i < len; i++)
			a[i] = isgraph(s[i]) ? s[i] : '.';
	up->syscalltrace = smprint("%d [%s] %s %#ux %d %#ux='%s' %d",
		up->pid, up->text,
		sysctab[ureg->ax], sp[0], sp[1], sp[2], a, sp[3]);
	break;
	}
	}
	
}

static void
retprint(Ureg *ureg, int syscallno)
{
	/* should never happen */
	if (up->syscalltrace)
		free(up->syscalltrace);
	switch(syscallno) {
		case SYSR1:
		case _ERRSTR:
		case BIND:
		case CHDIR:
		case CLOSE:
		case DUP:
		case ALARM:
		case EXEC:
		case EXITS:
		case _FSESSION:
		case FAUTH:
		case _FSTAT:
		case SEGBRK:
		case _MOUNT:
		case OPEN:
		case _READ:
		case OSEEK:
		case SLEEP:
		case _STAT:
		case _WRITE:
		case PIPE:
		case CREATE:
		case FD2PATH:
		case BRK_:
		case REMOVE:
		case _WSTAT:
		case _FWSTAT:
		case NOTIFY:
		case NOTED:
		case SEGATTACH:
		case SEGDETACH:
		case SEGFREE:
		case SEGFLUSH:
		case RENDEZVOUS:
		case UNMOUNT:
		case _WAIT:
		case SEMACQUIRE:
		case SEMRELEASE:
		case SEEK:
		case FVERSION:
		case ERRSTR:
		case STAT:
		case FSTAT:
		case WSTAT:
		case FWSTAT:
		case MOUNT:
		case AWAIT:
		case PWRITE:
		default: 
			if(ureg->ax == -1)
				up->syscalltrace = smprint("= %s\n", up->syserrstr);
			else
				up->syscalltrace = smprint("= %#ux\n", ureg->ax);
		break;
		case RFORK:
			if(ureg->ax == -1)
				up->syscalltrace = smprint("= %s\n", up->syserrstr);
			else
				up->syscalltrace = smprint("= %d\n", ureg->ax);
		break;
		case PREAD:
			if(ureg->ax == -1)
				up->syscalltrace = smprint("= %s\n", up->syserrstr);
			else {
				int len = up->s.args[2] > 64 ? 64 : up->s.args[2];
				char *s = uvalidaddr(up->s.args[1], len, 0);
				char a[65];
				int i;
				memset(a, 0, sizeof(a));
				for(i = 0; i < len; i++)
					a[i] = isgraph(s[i]) ? s[i] : '.';
				up->syscalltrace = smprint("= %#ux='%s'\n", ureg->ax, a);
			}
		break;
	}
	
}
/*
 * Handle a system call.
 */
static void
syscall(Ureg *ureg)
{
	char *e;
	ulong sp;
	uchar *usp;
	long	ret;
	int s;
	ulong scallnr;

	cycles(&up->kentry);
	m->syscall++;
	up->insyscall = 1;
	up->pc = ureg->pc;
	up->dbgreg = ureg;

	if(up->procctl == Proc_tracesyscall){
		up->procctl = Proc_stopme;
		syscallprint(ureg);
		procctl(up);
		if (up->syscalltrace)
			free(up->syscalltrace);
		up->syscalltrace = NULL;
	}

	scallnr = ureg->ax;
	up->scallnr = scallnr;
	if(scallnr == RFORK && up->fpstate == FPactive){
		fpsave(&up->fpsave);
		up->fpstate = FPinactive;
	}
	spllo();

	sp = ureg->usp;
	up->nerrlab = 0;
	ret = -1;
	if(!waserror()){
		if(scallnr >= nsyscall || systab[scallnr] == 0){
			pprint("bad sys call number %d pc %lux\n",
				scallnr, ureg->pc);
			postnote(up, 1, "sys: bad sys call", NDebug);
			error(Ebadarg);
		}

		
		usp = uvalidaddr(sp, sizeof(Sargs)+BY2WD, 0);
		up->s = *((Sargs*)(usp+BY2WD));
		up->psstate = sysctab[scallnr];

		ret = systab[scallnr](up->s.args);
		poperror();
	}else{
		/* failure: save the error buffer for errstr */
		e = up->syserrstr;
		up->syserrstr = up->errstr;
		up->errstr = e;
		if(0 && up->pid == 1)
			print("syscall %lud error %s\n", scallnr, up->syserrstr);
	}
	if(up->nerrlab){
		print("bad errstack [%lud]: %d extra\n", scallnr, up->nerrlab);
	//	for(i = 0; i < NERR; i++)
	//		print("sp=%lux pc=%lux\n",
	//XXX			up->errlab[i].sp, up->errlab[i].pc);
		panic("error stack");
	}

//	if(ret < 0)
//		print("%d [%s] %s\n", up->pid, up->psstate, up->syserrstr);
//	else
//		print("%d [%s] %d\n", up->pid, up->psstate, ret);
//	printmap();

	/*
	 *  Put return value in frame.  On the x86 the syscall is
	 *  just another trap and the return value from syscall is
	 *  ignored.  On other machines the return value is put into
	 *  the results register by caller of syscall.
	 */
	ureg->ax = ret;

	if(up->procctl == Proc_tracesyscall){
		up->procctl = Proc_stopme;
		retprint(ureg, scallnr);
		s = splhi();
		procctl(up);
		splx(s);
		/* you must have read the string by now. The free above is really not needed */
		if (up->syscalltrace)
			free(up->syscalltrace);
		up->syscalltrace = NULL;
	}

	up->insyscall = 0;
	up->psstate = 0;

	if(scallnr == NOTED)
		noted(ureg, *(uint32*)(up->pmmu.uzero + sp+BY2WD));

	if(scallnr!=RFORK && (up->procctl || up->nnote)){
		splhi();
		notify(ureg);
	}
	/* if we delayed sched because we held a lock, sched now */
	if(up->delaysched)
		sched();
	kexit(ureg);
}

/*
 *  Call user, if necessary, with note.
 *  Pass user the Ureg struct and the note on his stack.
 */
int
notify(Ureg* ureg)
{
	int l;
	ulong s, sp;
	Note *n;
	Ureg *upureg;

	if(tracesyscalls)
		iprint("notify\n");

	if(up->procctl)
		procctl(up);
	if(up->nnote == 0)
		return 0;

	if(up->fpstate == FPactive){
		fpsave(&up->fpsave);
		up->fpstate = FPinactive;
	}
	up->fpstate |= FPillegal;

	s = spllo();
	qlock(&up->debug);
	up->notepending = 0;
	n = &up->note[0];
	if(strncmp(n->msg, "sys:", 4) == 0){
		l = strlen(n->msg);
		if(l > ERRMAX-15)	/* " pc=0x12345678\0" */
			l = ERRMAX-15;
		sprint(n->msg+l, " pc=0x%.8lux", ureg->pc);
	}

	if(n->flag!=NUser && (up->notified || up->notify==0)){
		if(n->flag == NDebug)
			pprint("suicide: %s\n", n->msg);
		qunlock(&up->debug);
		pexit(n->msg, n->flag!=NDebug);
	}

	if(up->notified){
		qunlock(&up->debug);
		splhi();
		return 0;
	}
		
	if(!up->notify){
		qunlock(&up->debug);
		pexit(n->msg, n->flag!=NDebug);
	}
	sp = ureg->usp;
	sp -= sizeof(Ureg);

	if(!okaddr(up->notify, 1, 0)
	|| !okaddr(sp-ERRMAX-4*BY2WD, sizeof(Ureg)+ERRMAX+4*BY2WD, 1)){
		pprint("suicide: bad address in notify\n");
		qunlock(&up->debug);
		pexit("Suicide", 0);
	}

	uchar *uzero;
	uzero = up->pmmu.uzero;
	upureg = (void*)(uzero + sp);
	memmove(upureg, ureg, sizeof(Ureg));
	*(uint32*)(uzero + sp-BY2WD) = up->ureg;	/* word under Ureg is old up->ureg */
	up->ureg = sp;
	sp -= BY2WD+ERRMAX;
	memmove((char*)(uzero + sp), up->note[0].msg, ERRMAX);
	sp -= 3*BY2WD;
	*(uint32*)(uzero + sp+2*BY2WD) = sp+3*BY2WD;		/* arg 2 is string */
	*(uint32*)(uzero + sp+1*BY2WD) = up->ureg;	/* arg 1 is ureg* */
	*(uint32*)(uzero + sp+0*BY2WD) = 0;			/* arg 0 is pc */
	ureg->usp = sp;
	ureg->pc = up->notify;
	up->notified = 1;
	up->nnote--;
	memmove(&up->lastnote, &up->note[0], sizeof(Note));
	memmove(&up->note[0], &up->note[1], up->nnote*sizeof(Note));

	qunlock(&up->debug);
	splx(s);
	return 1;
}

/*
 *   Return user to state before notify()
 */
static void
noted(Ureg* ureg, ulong arg0)
{
	Ureg *nureg;
	ulong oureg, sp;

	qlock(&up->debug);
	if(arg0!=NRSTR && !up->notified) {
		qunlock(&up->debug);
		pprint("call to noted() when not notified\n");
		pexit("Suicide", 0);
	}
	up->notified = 0;

	up->fpstate &= ~FPillegal;

	/* sanity clause */
	if(!okaddr(up->ureg-BY2WD, BY2WD+sizeof(Ureg), 0)){
		pprint("bad ureg in noted or call to noted when not notified\n");
		qunlock(&up->debug);
		pexit("Suicide", 0);
	}
	
	uchar *uzero;
	uzero = up->pmmu.uzero;
	oureg = up->ureg;
	nureg = (Ureg*)(uzero + up->ureg);

	/* don't let user change system flags */
	nureg->flags = (ureg->flags & ~0xCD5) | (nureg->flags & 0xCD5);

	memmove(ureg, nureg, sizeof(Ureg));

	switch(arg0){
	case NCONT:
	case NRSTR:
		if(!okaddr(nureg->pc, 1, 0) || !okaddr(nureg->usp, BY2WD, 0)){
			qunlock(&up->debug);
			pprint("suicide: trap in noted\n");
			pexit("Suicide", 0);
		}
		up->ureg = *(uint32*)(uzero+oureg-BY2WD);
		qunlock(&up->debug);
		break;

	case NSAVE:
		if(!okaddr(nureg->pc, BY2WD, 0)
		|| !okaddr(nureg->usp, BY2WD, 0)){
			qunlock(&up->debug);
			pprint("suicide: trap in noted\n");
			pexit("Suicide", 0);
		}
		qunlock(&up->debug);
		sp = oureg-4*BY2WD-ERRMAX;
		splhi();
		ureg->sp = sp;
		((uint32*)(uzero+sp))[1] = oureg;	/* arg 1 0(FP) is ureg* */
		((uint32*)(uzero+sp))[0] = 0;		/* arg 0 is pc */
		break;

	default:
		pprint("unknown noted arg 0x%lux\n", arg0);
		up->lastnote.flag = NDebug;
		/* fall through */
		
	case NDFLT:
		if(up->lastnote.flag == NDebug){ 
			qunlock(&up->debug);
			pprint("suicide: %s\n", up->lastnote.msg);
		} else
			qunlock(&up->debug);
		pexit(up->lastnote.msg, up->lastnote.flag!=NDebug);
	}
}

long
execregs(ulong entry, ulong ssize, ulong nargs)
{
	uint32 *sp;
	Ureg *ureg;

	up->fpstate = FPinit;
	fpoff();

	sp = (uint32*)(up->pmmu.uzero + USTKTOP - ssize);
	*--sp = nargs;

	ureg = up->dbgreg;
	ureg->usp = (uchar*)sp - up->pmmu.uzero;
//showexec(ureg->usp);
	ureg->pc = entry;
	return USTKTOP-sizeof(Tos);		/* address of kernel/user shared data */
}

/*
 *  return the userpc the last exception happened at
 */
ulong
userpc(void)
{
	Ureg *ureg;

	ureg = (Ureg*)up->dbgreg;
	return ureg->pc;
}

/* This routine must save the values of registers the user is not permitted
 * to write from devproc and then restore the saved values before returning.
 */
void
setregisters(Ureg* ureg, char* pureg, char* uva, int n)
{
	ulong flags;

	flags = ureg->flags;
	memmove(pureg, uva, n);
	ureg->flags = (ureg->flags & 0x00FF) | (flags & 0xFF00);
}

static void
linkproc(void)
{
	spllo();
	up->kpfun(up->kparg);
	pexit("kproc dying", 0);
}

void
kprocchild(Proc* p, void (*func)(void*), void* arg)
{
	/*
	 * gotolabel() needs a word on the stack in
	 * which to place the return PC used to jump
	 * to linkproc().
	 */
	labelinit(&p->sched, (ulong)linkproc, (ulong)p->kstack+KSTACK-BY2WD);

	p->kpfun = func;
	p->kparg = arg;
}

void
forkchild(Proc *p, Ureg *ureg)
{
	Ureg *cureg;
	void *sp;

	/*
	 * Add 2*BY2WD to the stack to account for
	 *  - the return PC
	 *  - trap's argument (ur)
	 */
	sp = (char*)p->kstack+KSTACK-(sizeof(Ureg)+2*BY2WD);
	labelinit(&p->sched, (ulong)forkret, (ulong)sp);

	cureg = (Ureg*)((char*)sp+2*BY2WD);
	memmove(cureg, ureg, sizeof(Ureg));
	/* return value of syscall in child */
	cureg->ax = 0;

	/* Things from bottom of syscall which were never executed */
	p->psstate = 0;
	p->insyscall = 0;
}

/*
 * Give enough context in the ureg to produce a kernel stack for
 * a sleeping process.  Or not.
 */
void
setkernur(Ureg* ureg, Proc* p)
{
	memset(ureg, 0, sizeof *ureg);
}

ulong
dbgpc(Proc *p)
{
	Ureg *ureg;

	ureg = p->dbgreg;
	if(ureg == 0)
		return 0;

	return ureg->pc;
}

/*
 * floating point and all its glory.
 */

static char* mathmsg[] =
{
	nil,	/* handled below */
	"denormalized operand",
	"division by zero",
	"numeric overflow",
	"numeric underflow",
	"precision loss",
};

static void
mathnote(void)
{
	int i;
	ulong status;
	char *msg, note[ERRMAX];

	status = up->fpsave.status;

	/*
	 * Some attention should probably be paid here to the
	 * exception masks and error summary.
	 */
	msg = "unknown exception";
	for(i = 1; i <= 5; i++){
		if(!((1<<i) & status))
			continue;
		msg = mathmsg[i];
		break;
	}
	if(status & 0x01){
		if(status & 0x40){
			if(status & 0x200)
				msg = "stack overflow";
			else
				msg = "stack underflow";
		}else
			msg = "invalid operation";
	}
	sprint(note, "sys: fp: %s fppc=0x%lux status=0x%lux", msg, up->fpsave.pc, status);
	postnote(up, 1, note, NDebug);
}


/*
 *  math coprocessor emulation fault
 */
static void
mathemu(Ureg *ureg, void *v)
{
	if(up->fpstate & FPillegal){
		/* someone did floating point in a note handler */
		postnote(up, 1, "sys: floating point in note handler", NDebug);
		return;
	}
	switch(up->fpstate){
	case FPinit:
		fpinit();
		up->fpstate = FPactive;
		break;
	case FPinactive:
		/*
		 * Before restoring the state, check for any pending
		 * exceptions, there's no way to restore the state without
		 * generating an unmasked exception.
		 * More attention should probably be paid here to the
		 * exception masks and error summary.
		 */
		if((up->fpsave.status & ~up->fpsave.control) & 0x07F){
			mathnote();
			break;
		}
		fprestore(&up->fpsave);
		up->fpstate = FPactive;
		break;
	case FPactive:
		panic("math emu pid %ld %s pc 0x%lux", 
			up->pid, up->text, ureg->pc);
		break;
	}
}

/*
 *  math coprocessor segment overrun
 * TODO: When to call this?
 */
void
mathover(Ureg *u, void *v)
{
	pexit("math overrun", 0);
}

/*
 *  math coprocessor error
 * TODO: When to call this?
 */
void
matherror(Ureg *ur, void *v)
{
	/*
	 *  a write cycle to port 0xF0 clears the interrupt latch attached
	 *  to the error# line from the 387
	 */
//	if(!(m->cpuiddx & 0x01))
//		outb(0xF0, 0xFF);

	/*
	 *  save floating point state to check out error
	 */
	fpenv(&up->fpsave);
	mathnote();

	if(!okaddr(ur->pc, 1, 0))
		panic("fp: status %ux fppc=0x%lux pc=0x%lux",
			up->fpsave.status, up->fpsave.pc, ur->pc);
}

/*
 *  set up floating point for a new process
 */
void
procsetup(Proc*p)
{
	p->fpstate = FPinit;
	fpoff();
}

void
procrestore(Proc *p)
{
	uvlong t;

	if(p->kp)
		return;
	cycles(&t);
	p->pcycles -= t;
}

/*
 *  Save the mach dependent part of the process state.
 */
void
procsave(Proc *p)
{
	uvlong t;

	cycles(&t);
	p->pcycles += t;
	if(p->fpstate == FPactive){
		if(p->state == Moribund)
			fpclear();
		else{
			/*
			 * Fpsave() stores without handling pending
			 * unmasked exeptions. Postnote() can't be called
			 * here as sleep() already has up->rlock, so
			 * the handling of pending exceptions is delayed
			 * until the process runs again and generates an
			 * emulation fault to activate the FPU.
			 */
			fpsave(&p->fpsave);
		}
		p->fpstate = FPinactive;
	}
}

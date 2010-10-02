/*
 * Truly awful code to simulate Unix signal handler dispatch
 * using Mach signal handler dispatch.  The BSD support routines
 * can't deal with our SIGSEGVs properly.  Among other things,
 * they keep waking up other threads and they cause a popup 
 * about the application quitting when it hasn't.
 *
 * This code is inspired by similar code in SBCL.
 * See also http://paste.lisp.org/display/19593.
 * See also http://lists.apple.com/archives/darwin-dev/2006/Oct/msg00122.html
 */

#define __DARWIN_UNIX03 0

#include <mach/mach.h>
#include <sys/ucontext.h>
#include <pthread.h>
#include <signal.h>
#include <sys/signal.h>

#include "u.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

#define EFLAGS_TF 0x100

extern int invx32;

static x86_thread_state32_t normal;	/* normal segment registers */
static void *altstack;

static void (*sigbus)(int, siginfo_t*, void*);
static void (*sigtrap)(int, siginfo_t*, void*);
static void (*sigfpe)(int, siginfo_t*, void*);

/*
 * Manipulate stack in regs.
 */
static void
push(x86_thread_state32_t *regs, ulong data)
{
	uint *sp;
	
	sp = (uint*)regs->esp;
	*--sp = data;
	regs->esp = (uint)sp;
}

static void
align(x86_thread_state32_t *regs)
{
	uint *sp;
	
	sp = (uint*)regs->esp;
	while((ulong)sp & 15)
		sp--;
	regs->esp = (uint)sp;
}

static void*
alloc(x86_thread_state32_t *regs, int n)
{
	n = (n+15) & ~15;
	regs->esp -= n;
	return (void*)regs->esp;
}

/*
 * Signal handler wrapper.  Calls handler and then
 * causes an illegal instruction exception to jump
 * back to us.
 */
static void
wrapper(siginfo_t *siginfo,
	mcontext_t mcontext,
	void (*handler)(int, siginfo_t*, void*))
{
	ucontext_t ucontext;

	memset(&ucontext, 0, sizeof ucontext);
	ucontext.uc_mcontext = mcontext;
	handler(siginfo->si_signo, siginfo, &ucontext);

	/* Cause EXC_BAD_INSTRUCTION to "exit" signal handler */
	asm volatile(
		"movl %0, %%eax\n"
		"movl %1, %%ebx\n"
		"movl $0xdeadbeef, %%esp\n"
		".globl _wrapper_bad\n"
		"_wrapper_bad:\n"
		".long 0xffff0b0f\n"
		: : "r" (mcontext), "r" (siginfo));
}

void
dumpmcontext(mcontext_t m)
{
	x86_thread_state32_t *ureg;
	
	ureg = &m->ss;
	iprint("FLAGS=%luX TRAP=%luX ECODE=%luX PC=%luX CR2=%luX\n",
		ureg->eflags, m->es.trapno, m->es.err, ureg->eip, m->es.faultvaddr);
	iprint("  AX %8.8luX  BX %8.8luX  CX %8.8luX  DX %8.8luX\n",
		ureg->eax, ureg->ebx, ureg->ecx, ureg->edx);
	iprint("  SI %8.8luX  DI %8.8luX  BP %8.8luX  SP %8.8luX\n",
		ureg->esi, ureg->edi, ureg->ebp, ureg->esp);
}

void
dumpregs1(x86_thread_state32_t *ureg)
{
	iprint("FLAGS=%luX PC=%luX\n",
		ureg->eflags, ureg->eip);
	iprint("  AX %8.8luX  BX %8.8luX  CX %8.8luX  DX %8.8luX\n",
		ureg->eax, ureg->ebx, ureg->ecx, ureg->edx);
	iprint("  SI %8.8luX  DI %8.8luX  BP %8.8luX  SP %8.8luX\n",
		ureg->esi, ureg->edi, ureg->ebp, ureg->esp);
}


/*
 * Called by mach loop in exception handling thread.
 */
kern_return_t
catch_exception_raise(mach_port_t exception_port,
                      mach_port_t thread,
                      mach_port_t task,
                      exception_type_t exception,
                      exception_data_t code_vector,
                      mach_msg_type_number_t code_count)
{
	mach_msg_type_number_t n;
	x86_thread_state32_t regs, save_regs;
	siginfo_t *stk_siginfo;
	kern_return_t ret;
	uint *sp;
	mcontext_t stk_mcontext;
	void (*handler)(int, siginfo_t*, void*);
	ulong addr;
	int signo;

	n = x86_THREAD_STATE32_COUNT;
	ret = thread_get_state(thread, x86_THREAD_STATE32, (void*)&regs, &n);
	if(ret != KERN_SUCCESS)
		panic("mach get regs failed: %d", ret);

	addr = 0;
	save_regs = regs;

	switch(exception){
	case EXC_BAD_ACCESS:
		signo = SIGBUS;
		addr = code_vector[1];
		handler = sigbus;
		goto Trigger;

	case EXC_BREAKPOINT:
		signo = SIGTRAP;
		handler = sigtrap;
		regs.eflags &= ~EFLAGS_TF;
		goto Trigger;
		
	case EXC_ARITHMETIC:
		signo = SIGFPE;
		handler = sigfpe;
		goto Trigger;

	Trigger:
		if(invx32)
			regs.esp = (uint)altstack;
		else if(regs.ss != normal.ss)
			panic("not in vx32 but bogus ss");
		align(&regs);
		regs.cs = normal.cs;
		regs.ds = normal.ds;
		regs.es = normal.es;
		regs.ss = normal.ss;

		stk_siginfo = alloc(&regs, sizeof *stk_siginfo);
		stk_mcontext = alloc(&regs, sizeof *stk_mcontext);

		memset(stk_siginfo, 0, sizeof *stk_siginfo);
		stk_siginfo->si_signo = signo;
		stk_siginfo->si_addr = (void*)addr;

		stk_mcontext->ss = save_regs;
		n = x86_FLOAT_STATE32_COUNT;
		ret = thread_get_state(thread, x86_FLOAT_STATE32, (void*)&stk_mcontext->fs, &n);
		if(ret != KERN_SUCCESS)
			panic("mach get fpregs failed: %d", ret);
		n = x86_EXCEPTION_STATE32_COUNT;
		ret = thread_get_state(thread, x86_EXCEPTION_STATE32, (void*)&stk_mcontext->es, &n);
		if(ret != KERN_SUCCESS)
			panic("mach get eregs: %d", ret);

		sp = alloc(&regs, 3*4);
		sp[0] = (uint)stk_siginfo;
		sp[1] = (uint)stk_mcontext;
		sp[2] = (uint)handler;
		
		push(&regs, regs.eip);	/* for debugger; wrapper won't return */
		regs.eip = (uint)wrapper;

		ret = thread_set_state(thread, x86_THREAD_STATE32,
			(void*)&regs, x86_THREAD_STATE32_COUNT);
		if(ret != KERN_SUCCESS)
			panic("mach set regs failed: %d", ret);
		if(0 && stk_siginfo->si_signo != SIGBUS){
			iprint("call sig %d\n", stk_siginfo->si_signo);
			dumpmcontext(stk_mcontext);
		}
		return KERN_SUCCESS;
	
	case EXC_BAD_INSTRUCTION:;
		/*
		 * We use an invalid instruction in wrapper to note
		 * that we're done with the signal handler, but 
		 * Mach sends the same exception (different code_vector[0])
		 * when it gets the GP fault triggered by an address
		 * greater than the segment limit.  Catch both.
		 */
		extern char wrapper_bad[];
		if(regs.eip == (ulong)wrapper_bad && regs.esp == 0xdeadbeef){
			stk_mcontext = (mcontext_t)regs.eax;
			stk_siginfo = (siginfo_t*)regs.ebx;
			if(0 && stk_siginfo->si_signo != SIGBUS){
				iprint("return sig %d\n", stk_siginfo->si_signo);
				dumpmcontext(stk_mcontext);
			}
			ret = thread_set_state(thread, x86_THREAD_STATE32,
				(void*)&stk_mcontext->ss, x86_THREAD_STATE32_COUNT);
			if(ret != KERN_SUCCESS)
				panic("mach set regs1 failed: %d", ret);
			ret = thread_set_state(thread, x86_FLOAT_STATE32,
				(void*)&stk_mcontext->fs, x86_FLOAT_STATE32_COUNT);
			if(ret != KERN_SUCCESS)
				panic("mach set fpregs failed: %d", ret);
			return KERN_SUCCESS;
		}

		/*
		 * Other things can cause GP faults too, but let's assume it was this.
		 * Linux sends si_addr == 0; so will we.  The app isn't going to try
		 * to recover anyway, so it's not a big deal if we send other GP
		 * faults that way too.
		 */
		if(code_vector[0] == EXC_I386_GPFLT){
			signo = SIGBUS;
			handler = sigbus;
			addr = 0;
			goto Trigger;
		}

		iprint("Unexpected bad instruction at eip=%p: code %p %p\n",
			regs.eip, code_vector[0], code_vector[1]);
		dumpregs1(&regs);
		return KERN_INVALID_RIGHT;
	}
	return KERN_INVALID_RIGHT;
}

static void*
handler(void *v)
{
	extern boolean_t exc_server();
	mach_port_t port;
	
	setmach(machp[0]);
	port = (mach_port_t)v;
	mach_msg_server(exc_server, 2048, port, 0);
	return 0;	/* not reached */
}

void
machsiginit(void)
{
	mach_port_t port;
	pthread_t pid;
	stack_t ss;
	struct sigaction act;
	int ret;
	
	extern int vx32_getcontext(x86_thread_state32_t*);
	vx32_getcontext(&normal);

	if(sigaltstack(nil, &ss) < 0 || (ss.ss_flags & SS_DISABLE))
		panic("machsiginit: no alt stack");
	altstack = ss.ss_sp + ss.ss_size;

	if(sigaction(SIGBUS, nil, &act) < 0)
		panic("sigaction bus");
	sigbus = (void*)act.sa_handler;

	if(sigaction(SIGTRAP, nil, &act) < 0)
		panic("sigaction trap");
	sigtrap = (void*)act.sa_handler;

	if(sigaction(SIGFPE, nil, &act) < 0)
		panic("sigaction fpe");
	sigfpe = (void*)act.sa_handler;

	mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
	mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
	ret = thread_set_exception_ports(mach_thread_self(),
		EXC_MASK_BAD_ACCESS|EXC_MASK_BAD_INSTRUCTION|
		EXC_MASK_BREAKPOINT|EXC_MASK_ARITHMETIC,
		port, EXCEPTION_DEFAULT, MACHINE_THREAD_STATE);
	pthread_create(&pid, nil, handler, (void*)port);
}


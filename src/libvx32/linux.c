// Code specific to x86 hosts running Linux.

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <ucontext.h>
#include <asm/ldt.h>

#include "vx32.h"
#include "vx32impl.h"
#include "os.h"

extern int modify_ldt(int, void*, unsigned long);

int vxemu_map(vxemu *emu, vxmmap *mm)
{
	struct vxproc *vxp;
	struct user_desc desc;
#ifdef __x86_64
	static int didflat;
#endif

	vxp = emu->proc;
	emu->datasel = vxp->vxpno * 16 + 16 + 4 + 3;	// 4=LDT, 3=RPL
	emu->emusel = emu->datasel + 8;

	if (emu->ldt_base != (uintptr_t)mm->base || emu->ldt_size != mm->size) {
		// Set up the process's data segment selector (for DS,ES,SS).
		memset(&desc, 0, sizeof(desc));
		desc.seg_32bit = 1;
		desc.read_exec_only = 0;
		desc.limit_in_pages = 1;
		desc.seg_not_present = 0;
		desc.useable = 1;

		desc.entry_number = emu->datasel / 8;
		desc.base_addr = (uintptr_t)mm->base;
		desc.limit = (mm->size - 1) >> VXPAGESHIFT;
		desc.contents = MODIFY_LDT_CONTENTS_DATA;
		if (modify_ldt(1, &desc, sizeof(desc)) < 0)
			return -1;
	
		// Set up the process's vxemu segment selector (for FS).
		desc.entry_number = emu->emusel / 8;
		desc.base_addr = (uintptr_t)emu;
		desc.limit = (VXCODEBUFSIZE - 1) >> VXPAGESHIFT;
		desc.contents = MODIFY_LDT_CONTENTS_DATA;
		if (modify_ldt(1, &desc, sizeof(desc)) < 0)
			return -1;
		
		emu->ldt_base = (uintptr_t)mm->base;
		emu->ldt_size = mm->size;
	}

#ifdef __x86_64
	// Set up 32-bit mode code and data segments (not vxproc-specific),
	// giving access to the full low 32 bits of linear address space.
	// The code segment is necessary to get into 32-bit compatibility mode;
	// the data segment is needed because Linux for x86-64
	// doesn't give 64-bit processes a "real" data segment by default
	// but instead just loads zero into the data segment selectors!
	emu->runptr.sel = FLATCODE;

	if (!didflat) {
		didflat = 1;
		memset(&desc, 0, sizeof(desc));
		desc.seg_32bit = 1;
		desc.read_exec_only = 0;
		desc.limit_in_pages = 1;
		desc.seg_not_present = 0;
		desc.useable = 1;

		desc.entry_number = emu->runptr.sel / 8;
		desc.base_addr = 0;
		desc.limit = 0xfffff;
		desc.contents = MODIFY_LDT_CONTENTS_CODE;
		if (modify_ldt(1, &desc, sizeof(desc)) < 0)
			return -1;
	
		desc.entry_number = FLATDATA / 8;
		desc.base_addr = 0;
		desc.limit = 0xfffff;
		desc.contents = MODIFY_LDT_CONTENTS_DATA;
		if (modify_ldt(1, &desc, sizeof(desc)) < 0)
			return -1;
	}

	// Set up a far return vector in emu->retptr
	// for getting back into 64-bit long mode.
	extern void vxrun_return();
	asm volatile("movw %%cs,%0" : "=r" (emu->retptr.sel));
	emu->retptr.ofs = (uint32_t)(intptr_t)vxrun_return;
#endif

	return 0;
}

static void dumpsigcontext(struct sigcontext *ctx)
{
#ifdef i386
	printf(
		"eax %08lx  ebx %08lx\necx %08lx  edx %08lx  "
		"rsi %08lx  rdi %08lx\nrbp %08lx  rsp %08lx\n"
		"eip %08lx  efl %08lx  cs %04x\n"
		"err %08lx  trapno %08lx  cr2 %08lx\n",
		ctx->eax, ctx->ebx, ctx->ecx, ctx->edx,
		ctx->esi, ctx->edi, ctx->ebp, ctx->esp,
		ctx->eip, ctx->eflags, ctx->cs,
		ctx->err, ctx->trapno, ctx->cr2);
#else
	printf(
		"rax %016lx  rbx %016lx\nrcx %016lx  rdx %016lx\n"
		"rsi %016lx  rdi %016lx\nrbp %016lx  rsp %016lx\n"
		"r8  %016lx  r9  %016lx\nr10 %016lx  r11 %016lx\n"
		"r12 %016lx  r13 %016lx\nr14 %016lx  r15 %016lx\n"
		"rip %016lx  efl %016lx  cs %04x  ss %04x\n"
		"err %016lx  trapno %016lx  cr2 %016lx\n",
		ctx->rax, ctx->rbx, ctx->rcx, ctx->rdx,
		ctx->rsi, ctx->rdi, ctx->rbp, ctx->rsp,
		ctx->r8, ctx->r9, ctx->r10, ctx->r11,
		ctx->r12, ctx->r13, ctx->r14, ctx->r15,
		ctx->rip, ctx->eflags, ctx->cs, ctx->__pad0,
		ctx->err, ctx->trapno, ctx->cr2);
#endif
}

#ifdef i386
#define	VX32_BELIEVE_EIP	(ctx->ds == vs - 8)
#else
#define	VX32_BELIEVE_EIP	(ctx->cs == FLATCODE)

// On x86-64, make x86 names work for ctx->xxx.
#define	eax rax
#define	ebx rbx
#define	ecx rcx
#define	edx rdx
#define	esi rsi
#define	edi rdi
#define	esp rsp
#define	ebp rbp
#define	eip rip
#endif

static void
fprestore(struct _fpstate *s)
{
	asm volatile("frstor 0(%%eax); fwait\n" : : "a" (s) : "memory");
}

int vx32_sighandler(int signo, siginfo_t *si, void *v)
{
	uint32_t trapeip;
	uint32_t magic;
	uint16_t vs;
	vxproc *vxp;
	vxemu *emu;
	struct sigcontext *ctx;
	ucontext_t *uc;
	mcontext_t *mc;
	int r;

	uc = v;
	mc = &uc->uc_mcontext;

	// same layout, and sigcontext is more convenient...
	ctx = (struct sigcontext*)mc;

	// We can't be sure that vxemu is running,
	// and thus that %VSEG is actually mapped to a
	// valid vxemu.  The only way to tell is to look at %VSEG.

	// First sanity check vxproc segment number.
	asm("movw %"VSEGSTR",%0"
		: "=r" (vs));
	
	if(0) vxprint("vx32_sighandler signo=%d eip=%#x esp=%#x vs=%#x\n",
		signo, ctx->eip, ctx->esp, vs);

	if ((vs & 15) != 15)	// 8 (emu), LDT, RPL=3
		return 0;

	// Okay, assume mapped; check for vxemu.
	asm("movl %"VSEGSTR":%1,%0"
		: "=r" (magic)
		: "m" (((vxemu*)0)->magic));
	if (magic != VXEMU_MAGIC)
		return 0;

	// Okay, we're convinced.

	// Find current vxproc and vxemu.
	asm("mov %"VSEGSTR":%1,%0"
		: "=r" (vxp)
		: "m" (((vxemu*)0)->proc));
	emu = vxp->emu;

	// Get back our regular host segment register state,
	// so that thread-local storage and such works.
	vxrun_cleanup(emu);

	// dumpsigcontext(ctx);

	if (VX32_BELIEVE_EIP)
		trapeip = ctx->eip;
	else
		trapeip = 0xffffffff;

	int newtrap;
	switch(signo){
	case SIGSEGV:
	case SIGBUS:
		newtrap = VXTRAP_PAGEFAULT;
		break;
	
	case SIGFPE:
		newtrap = VXTRAP_FLOAT;
		break;
	
	case SIGVTALRM:
		newtrap = VXTRAP_IRQ + VXIRQ_TIMER;
		break;

	case SIGTRAP:
		// Linux sends SIGTRAP when it gets a processor 
		// debug exception, which is caused by single-stepping
		// with the TF bit, among other things.  The processor
		// turns off the TF bit before generating the trap, but
		// it appears that Linux turns it back on for us.
		// Let's use it to confirm that this is a single-step trap.
		if (ctx->eflags & EFLAGS_TF){
			newtrap = VXTRAP_SINGLESTEP;
			ctx->eflags &= ~EFLAGS_TF;
		}else{
			vxprint("Unexpected sigtrap eflags=%#x\n", ctx->eflags);
			newtrap = VXTRAP_SIGNAL + signo;
		}
		break;

	default:
		newtrap = VXTRAP_SIGNAL + signo;
		break;
	}
	
	int replaced_trap = 0;
	if (emu->cpu_trap) {
		// There's already a pending trap!
		// Handle the new trap, and assume that when it
		// finishes, restarting the code at cpu.eip will trigger
		// the old trap again.
		// Have to fix up eip for int 0x30 and syscall instructions.
		if (emu->cpu_trap == VXTRAP_SYSCALL ||
				(emu->cpu_trap&VXTRAP_CATEGORY) == VXTRAP_SOFT)
			emu->cpu.eip -= 2;
		replaced_trap = emu->cpu_trap;
	}
	emu->cpu_trap = newtrap;

	r = vxemu_sighandler(emu, trapeip);

	if (r == VXSIG_SINGLESTEP){
		// Vxemu_sighandler wants us to single step.
		// Execution state is in intermediate state - don't touch.
		ctx->eflags |= EFLAGS_TF;		// x86 TF (single-step) bit
		vxrun_setup(emu);
		return 1;
	}

	// Copy execution state into emu.
	if ((r & VXSIG_SAVE_ALL) == VXSIG_SAVE_ALL) {
		emu->cpu.reg[EAX] = ctx->eax;
		emu->cpu.reg[EBX] = ctx->ebx;
		emu->cpu.reg[ECX] = ctx->ecx;
		emu->cpu.reg[EDX] = ctx->edx;
		emu->cpu.reg[ESI] =  ctx->esi;
		emu->cpu.reg[EDI] = ctx->edi;
		emu->cpu.reg[ESP] = ctx->esp;	// or esp_at_signal ???
		emu->cpu.reg[EBP] = ctx->ebp;
		emu->cpu.eflags = ctx->eflags;
	} else if (r & VXSIG_SAVE_ALL) {
		if (r & VXSIG_SAVE_EAX)
			emu->cpu.reg[EAX] = ctx->eax;
		if (r & VXSIG_SAVE_EBX)
			emu->cpu.reg[EBX] = ctx->ebx;
		if (r & VXSIG_SAVE_ECX)
			emu->cpu.reg[ECX] = ctx->ecx;
		if (r & VXSIG_SAVE_EDX)
			emu->cpu.reg[EDX] = ctx->edx;
		if (r & VXSIG_SAVE_ESI)
			emu->cpu.reg[ESI] =  ctx->esi;
		if (r & VXSIG_SAVE_EDI)
			emu->cpu.reg[EDI] = ctx->edi;
		if (r & VXSIG_SAVE_ESP)
			emu->cpu.reg[ESP] = ctx->esp;	// or esp_at_signal ???
		if (r & VXSIG_SAVE_EBP)
			emu->cpu.reg[EBP] = ctx->ebp;
		if (r & VXSIG_SAVE_EFLAGS)
			emu->cpu.eflags = ctx->eflags;
	}
	r &= ~VXSIG_SAVE_ALL;

	if (r & VXSIG_SAVE_EBX_AS_EIP)
		emu->cpu.eip = ctx->ebx;
	r &= ~VXSIG_SAVE_EBX_AS_EIP;

	if (r & VXSIG_ADD_COUNT_TO_ESP) {
		emu->cpu.reg[ESP] += (uint16_t)(r >> VXSIG_COUNT_SHIFT);
		r &= ~VXSIG_ADD_COUNT_TO_ESP;
		r &= ~(0xFFFF << VXSIG_COUNT_SHIFT);
	}
	
	if (r &  VXSIG_INC_ECX) {
		emu->cpu.reg[ECX]++;
		r &= ~VXSIG_INC_ECX;
	}

	if (r == VXSIG_TRAP) {
		if (emu->trapenv == NULL)
			return 0;
		emu->cpu.traperr = ctx->err;
		emu->cpu.trapva = ctx->cr2;

		/*
		 * Linux helpfully reset the floating point state
		 * before entering the signal hander, so change it back.
		  */
		if(ctx->fpstate)
			fprestore(ctx->fpstate);
		siglongjmp(*emu->trapenv, 1);
	}

	// The signal handler is confused; so are we.
	return 0;
}

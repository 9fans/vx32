// Code specific to x86 hosts running Mac OS X

// get access to mcontext and thread_state fields with their normal names,
// not with everything prefixed by __ due to namespace cleanliness paranoia
#define __DARWIN_UNIX03 0

#include <string.h>
#include <signal.h>
#include <assert.h>
#include <ucontext.h>
#include <ucontext.h>
#include <architecture/i386/table.h>
#include <i386/user_ldt.h>

#include "vx32.h"
#include "vx32impl.h"
#include "os.h"

#if __LP64__
#error "vx32 on Darwin x86-64 is unimplemented."
#endif

// First LDT selector number to use for our translator segments.
// Avoid stepping on the low selectors used by the kernel.
// XXX is there a way to know how many selectors the kernel uses?
#define SELOFS	((32*8) + 4 + 3)		// 4=LDT, 3=RPL


static void setbase(union ldt_entry *desc, unsigned long base)
{
	desc->code.base00 = base & 0xffff;
	desc->code.base16 = (base >> 16) & 0xff;
	desc->code.base24 = base >> 24;
}

static void setlimit(union ldt_entry *desc, unsigned long limit)
{
	desc->code.limit00 = limit & 0xffff;
	desc->code.limit16 = limit >> 16;
}


// Set up LDT segments needed by the instruction translater
// whenever a vx32 process's memory is mapped.
int vxemu_map(vxemu *emu, vxmmap *mm)
{
	int s, sel;
	struct vxproc *vxp;
	union ldt_entry desc;
	
	vxp = emu->proc;

	if (emu->ldt_base != (uintptr_t)mm->base || emu->ldt_size != mm->size) {
		// Set up the process's data segment selector (for DS,ES,SS).
		memset(&desc, 0, sizeof(desc));
		setbase(&desc, (unsigned long)mm->base);
		setlimit(&desc, (mm->size - 1) >> VXPAGESHIFT);
		desc.data.type = DESC_DATA_WRITE;
		desc.data.dpl = 3;
		desc.data.present = 1;
		desc.data.stksz = DESC_DATA_32B;
		desc.data.granular = 1;
		if(emu->datasel == 0){
			if ((s = i386_set_ldt(LDT_AUTO_ALLOC, &desc, 1)) < 0)
				return -1;
			emu->datasel = (s<<3) + 4 + 3;	// 4=LDT, 3=RPL
		}else if(i386_set_ldt(emu->datasel >> 3, &desc, 1) < 0)
			return -1;

		// Set up the process's vxemu segment selector (for FS).
		setbase(&desc, (unsigned long)emu);
		setlimit(&desc, (VXCODEBUFSIZE - 1) >> VXPAGESHIFT);
		if(emu->emusel == 0){
			if ((s = i386_set_ldt(LDT_AUTO_ALLOC, &desc, 1)) < 0)
				return -1;
			emu->emusel = (s<<3) + 4 + 3;	// 4=LDT, 3=RPL
		}else if(i386_set_ldt(emu->emusel >> 3, &desc, 1) < 0)
			return -1;

		emu->ldt_base = (uintptr_t)mm->base;
		emu->ldt_size = mm->size;
	}

	return 0;
}

// Note: it's not generally safe to call printf from here,
// even just for debugging, because we're running on a small signal stack.
// Vxprint is an attempt to use less stack space but
// even that is not a sure thing.
int vx32_sighandler(int signo, siginfo_t *si, void *v)
{
	int r;
	uint32_t magic;
	uint16_t vs, oldvs;
	vxproc *vxp;
	vxemu *emu;
	ucontext_t *uc;
	mcontext_t ctx;
	uint32_t cr2;

	// In Darwin, 'mcontext_t' is actually a pointer...
	uc = v;
	ctx = uc->uc_mcontext;

	// We can't be sure that vxemu is running,
	// and thus that %VSEG is actually mapped to a
	// valid vxemu.  The only way to tell is to look at %VSEG.

	// First sanity check vxproc segment number.
	// Darwin reset the register before entering the handler!
	asm("movw %"VSEGSTR",%0"
		: "=r" (oldvs));
	vs = ctx->ss_vs & 0xFFFF;	/* ss_vs #defined in os.h */

	if(0) vxprint("vx32_sighandler signo=%d eip=%#x esp=%#x vs=%#x currentvs=%#x\n",
		signo, ctx->ss.eip, ctx->ss.esp, vs, oldvs);

	if ((vs & 7) != 7)	// LDT, RPL=3
		return 0;

	// Okay, assume mapped; check for vxemu by reading
	// first word from vs.  Have to put vs into the segment
	// register and then take it back out.
	asm("movw %"VSEGSTR",%1\n"
		"movw %2,%"VSEGSTR"\n"
		"movl %"VSEGSTR":%3,%0\n"
		"movw %1,%"VSEGSTR"\n"
		: "=r" (magic), "=r" (oldvs)
		: "r" (vs), "m" (((vxemu*)0)->magic));
	if (magic != VXEMU_MAGIC)
		return 0;

	// Okay, we're convinced.

	// Find current vxproc and vxemu.
	asm("movw %"VSEGSTR",%1\n"
		"movw %2,%"VSEGSTR"\n"
		"movl %"VSEGSTR":%3,%0\n"
		"movw %1,%"VSEGSTR"\n"
		: "=r" (vxp), "=r" (oldvs)
		: "r" (vs), "m" (((vxemu*)0)->proc));
	emu = vxp->emu;

	// Get back our regular host segment register state,
	// so that thread-local storage and such works.
	vxrun_cleanup(emu);

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
		// OS X sends SIGTRAP when it gets a processor 
		// debug exception, which is caused by single-stepping
		// with the TF bit, among other things.  The processor
		// turns off the TF bit before generating the trap, but
		// it appears that the handler turns it back on for us.
		// Let's use it to confirm that this is a single-step trap.
		if (ctx->ss.eflags & EFLAGS_TF){
			newtrap = VXTRAP_SINGLESTEP;
			ctx->ss.eflags &= ~EFLAGS_TF;
		}else{
			vxprint("Unexpected sigtrap eflags=%#x\n", ctx->ss.eflags);
			newtrap = VXTRAP_SIGNAL + signo;
		}
		break;

	default:
		newtrap = VXTRAP_SIGNAL + signo;
		break;
	}
	
	if (emu->cpu_trap) {
		// There's already a pending trap!
		// Handle the new trap, and assume that when it
		// finishes, restarting the code at cpu.eip will trigger
		// the old trap again.
		// Have to fix up eip for int 0x30 and syscall instructions.
		if (emu->cpu_trap == VXTRAP_SYSCALL ||
				(emu->cpu_trap&VXTRAP_CATEGORY) == VXTRAP_SOFT)
			emu->cpu.eip -= 2;
	}
	emu->cpu_trap = newtrap;

	r = vxemu_sighandler(emu, ctx->ss.eip);
	
	if (r == VXSIG_SINGLESTEP){
		// Vxemu_sighandler wants us to single step.
		// Execution state is in intermediate state - don't touch.
		ctx->ss.eflags |= EFLAGS_TF;		// x86 TF (single-step) bit
		vxrun_setup(emu);
		return 1;
	}

	// Copy execution state into emu.
	if ((r & VXSIG_SAVE_ALL) == VXSIG_SAVE_ALL) {
		emu->cpu.reg[EAX] = ctx->ss.eax;
		emu->cpu.reg[EBX] = ctx->ss.ebx;
		emu->cpu.reg[ECX] = ctx->ss.ecx;
		emu->cpu.reg[EDX] = ctx->ss.edx;
		emu->cpu.reg[ESI] =  ctx->ss.esi;
		emu->cpu.reg[EDI] = ctx->ss.edi;
		emu->cpu.reg[ESP] = ctx->ss.esp;	// or esp_at_signal ???
		emu->cpu.reg[EBP] = ctx->ss.ebp;
		emu->cpu.eflags = ctx->ss.eflags;
	} else if (r & VXSIG_SAVE_ALL) {
		if (r & VXSIG_SAVE_EAX)
			emu->cpu.reg[EAX] = ctx->ss.eax;
		if (r & VXSIG_SAVE_EBX)
			emu->cpu.reg[EBX] = ctx->ss.ebx;
		if (r & VXSIG_SAVE_ECX)
			emu->cpu.reg[ECX] = ctx->ss.ecx;
		if (r & VXSIG_SAVE_EDX)
			emu->cpu.reg[EDX] = ctx->ss.edx;
		if (r & VXSIG_SAVE_ESI)
			emu->cpu.reg[ESI] =  ctx->ss.esi;
		if (r & VXSIG_SAVE_EDI)
			emu->cpu.reg[EDI] = ctx->ss.edi;
		if (r & VXSIG_SAVE_ESP)
			emu->cpu.reg[ESP] = ctx->ss.esp;	// or esp_at_signal ???
		if (r & VXSIG_SAVE_EBP)
			emu->cpu.reg[EBP] = ctx->ss.ebp;
		if (r & VXSIG_SAVE_EFLAGS)
			emu->cpu.eflags = ctx->ss.eflags;
	}
	r &= ~VXSIG_SAVE_ALL;

	if (r & VXSIG_SAVE_EBX_AS_EIP)
		emu->cpu.eip = ctx->ss.ebx;
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
		emu->cpu.traperr = ctx->es.err;
		emu->cpu.trapva = ctx->es.faultvaddr;
		ctx->ss = *emu->trapenv;
		return 1;
	}

	// The signal handler is confused; so are we.
	return 0;
}


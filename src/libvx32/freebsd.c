// Code specific to x86 hosts running FreeBSD.

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <ucontext.h>
#include <machine/ucontext.h>

#include <machine/segments.h>
#include <machine/sysarch.h>

#include "vx32.h"
#include "vx32impl.h"
#include "os.h"

#if __FreeBSD__ < 7
#warning "libvx32 and FreeBSD 5 and 6's libpthread are not compatible."
#endif

static void setbase(struct segment_descriptor *desc, unsigned long base)
{
	desc->sd_lobase = base & 0xffffff;
	desc->sd_hibase = base >> 24;
}

static void setlimit(struct segment_descriptor *desc, unsigned long limit)
{
	desc->sd_lolimit = limit & 0xffff;
	desc->sd_hilimit = limit >> 16;
}


int vxemu_map(vxemu *emu, vxmmap *mm)
{
	int s, sel;
	struct vxproc *vxp;
	union descriptor desc;
	
	vxp = emu->proc;

	if (emu->ldt_base != (uintptr_t)mm->base || emu->ldt_size != mm->size) {
		// Set up the process's data segment selector (for DS,ES,SS).
		memset(&desc, 0, sizeof(desc));
		setbase(&desc.sd, (unsigned long)mm->base);
		setlimit(&desc.sd, (mm->size - 1) >> VXPAGESHIFT);
		desc.sd.sd_type = SDT_MEMRWA;
		desc.sd.sd_dpl = 3;
		desc.sd.sd_p = 1;
		desc.sd.sd_def32 = 1;
		desc.sd.sd_gran = 1;
		if(emu->datasel == 0){
			if ((s = i386_set_ldt(LDT_AUTO_ALLOC, &desc, 1)) < 0)
				return -1;
			emu->datasel = (s<<3) + 4 + 3;	// 4=LDT, 3=RPL
		}else if(i386_set_ldt(emu->datasel >> 3, &desc, 1) < 0)
			return -1;

		// Set up the process's vxemu segment selector (for FS).
		setbase(&desc.sd, (unsigned long)emu);
		setlimit(&desc.sd, (VXCODEBUFSIZE - 1) >> VXPAGESHIFT);
		if(emu->emusel == 0){
			if ((s = i386_set_ldt(LDT_AUTO_ALLOC, &desc, 1)) < 0)
				return -1;
			emu->emusel = (s<<3) + 4 + 3;	// 4=LDT, 3=RPL
		}else if(i386_set_ldt(emu->emusel >> 3, &desc, 1) < 0)
			return -1;

		emu->ldt_base = (uintptr_t)mm->base;
		emu->ldt_size = mm->size;
	}

#ifdef __x86_64
	// Set up 32-bit mode code and data segments (not vxproc-specific),
	// giving access to the full low 32-bit of linear address space.
	// The code segment is necessary to get into 32-bit compatibility mode;
	// the data segment is needed because Linux for x86-64
	// doesn't give 64-bit processes a "real" data segment by default
	// but instead just loads zero into the data segment selectors!
	emu->runptr.sel = FLATCODE;
	desc.entry_number = emu->runptr.sel / 8;
	desc.base_addr = 0;
	desc.limit = 0xfffff;
	desc.contents = MODIFY_LDT_CONTENTS_CODE;
	if (modify_ldt(1, &desc, sizeof(desc)) < 0)
		return -1;

	desc.entry_number = FLATDATA / 8;
	desc.contents = MODIFY_LDT_CONTENTS_DATA;
	if (modify_ldt(1, &desc, sizeof(desc)) < 0)
		return -1;

	// Set up a far return vector in emu->retptr
	// for getting back into 64-bit long mode.
	extern void vxrun_return();
	asm volatile("movw %%cs,%0" : "=r" (emu->retptr.sel));
	emu->retptr.ofs = (uint32_t)(intptr_t)vxrun_return;
#endif

	return 0;
}

static void dumpmcontext(mcontext_t *ctx, uint32_t cr2)
{
#ifdef i386
	vxprint(
		"eax %08x  ebx %08x ecx %08x  edx %08x\n"
		"esi %08x  edi %08x ebp %08x  esp %08x\n"
		"eip %08x  efl %08x  cs %04x\n"
		"err %08x  trapno %08x  cr2 %08x\n",
		ctx->mc_eax, ctx->mc_ebx, ctx->mc_ecx, ctx->mc_edx,
		ctx->mc_esi, ctx->mc_edi, ctx->mc_ebp, ctx->mc_esp,
		ctx->mc_eip, ctx->mc_eflags, ctx->mc_cs,
		ctx->mc_err, ctx->mc_trapno, cr2);
#else
	vxprint(
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

static void
fprestore(int *state, int fmt)
{
	if(fmt == _MC_FPFMT_387)
		asm volatile("frstor 0(%%eax); fwait\n" : : "a" (state) : "memory");
	else if(fmt == _MC_FPFMT_XMM){
		/* Have to 16-align the 512-byte state */
		char buf[512+16], *p;
		p = buf;
		if((long)p&15)
			p += 16 - (long)p&15;
		memmove(p, state, 512);
		asm volatile("fxrstor 0(%%eax); fwait\n" : : "a" (p) : "memory");
	}else
		abort();
}

int vx32_sighandler(int signo, siginfo_t *si, void *v)
{
	int r;
	uint32_t magic;
	uint16_t vs, oldvs;
	vxproc *vxp;
	vxemu *emu;
	ucontext_t *uc;
	mcontext_t *mc;
	uint32_t cr2;

	uc = v;
	mc = &uc->uc_mcontext;

	cr2 = (uint32_t)si->si_addr;

	// We can't be sure that vxemu is running,
	// and thus that %VSEG is actually mapped to a
	// valid vxemu.  The only way to tell is to look at %VSEG.

	// First sanity check vxproc segment number.
	// FreeBSD reset the register before entering the handler!
	asm("movw %"VSEGSTR",%0"
		: "=r" (oldvs));
	vs = mc->mc_vs & 0xFFFF;	/* mc_vs #defined in os.h */

	if(0) vxprint("vx32_sighandler signo=%d eip=%#x esp=%#x vs=%#x currentvs=%#x\n",
		signo, mc->mc_eip, mc->mc_esp, vs, oldvs);

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
	
	// dumpmcontext(mc, cr2);

	int newtrap;
	switch(signo){
	case SIGSEGV:
	case SIGBUS:
		newtrap = VXTRAP_PAGEFAULT;
		break;
	
	case SIGFPE:
		// iprint("fpe %d\n", si->si_code);
		newtrap = VXTRAP_FLOAT;
		break;
	
	case SIGVTALRM:
		newtrap = VXTRAP_IRQ + VXIRQ_TIMER;
		break;

	case SIGTRAP:
#warning "FreeBSD: need to test single-stepping"
		// Linux sends SIGTRAP when it gets a processor 
		// debug exception, which is caused by single-stepping
		// with the TF bit, among other things.  The processor
		// turns off the TF bit before generating the trap, but
		// it appears that Linux turns it back on for us.
		// Let's use it to confirm that this is a single-step trap.
		if (mc->mc_eflags & EFLAGS_TF){
			newtrap = VXTRAP_SINGLESTEP;
			mc->mc_eflags &= ~EFLAGS_TF;
		}else{
			vxprint("Unexpected sigtrap eflags=%#x\n", mc->mc_eflags);
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

	r = vxemu_sighandler(emu, mc->mc_eip);

	if (r == VXSIG_SINGLESTEP){
		// Vxemu_sighandler wants us to single step.
		// Execution state is in intermediate state - don't touch.
		mc->mc_eflags |= EFLAGS_TF;		// x86 TF (single-step) bit
		vxrun_setup(emu);
		return 1;
	}

	// Copy execution state into emu.
	if ((r & VXSIG_SAVE_ALL) == VXSIG_SAVE_ALL) {
		emu->cpu.reg[EAX] = mc->mc_eax;
		emu->cpu.reg[EBX] = mc->mc_ebx;
		emu->cpu.reg[ECX] = mc->mc_ecx;
		emu->cpu.reg[EDX] = mc->mc_edx;
		emu->cpu.reg[ESI] =  mc->mc_esi;
		emu->cpu.reg[EDI] = mc->mc_edi;
		emu->cpu.reg[ESP] = mc->mc_esp;	// or esp_at_signal ???
		emu->cpu.reg[EBP] = mc->mc_ebp;
		emu->cpu.eflags = mc->mc_eflags;
	} else if (r & VXSIG_SAVE_ALL) {
		if (r & VXSIG_SAVE_EAX)
			emu->cpu.reg[EAX] = mc->mc_eax;
		if (r & VXSIG_SAVE_EBX)
			emu->cpu.reg[EBX] = mc->mc_ebx;
		if (r & VXSIG_SAVE_ECX)
			emu->cpu.reg[ECX] = mc->mc_ecx;
		if (r & VXSIG_SAVE_EDX)
			emu->cpu.reg[EDX] = mc->mc_edx;
		if (r & VXSIG_SAVE_ESI)
			emu->cpu.reg[ESI] =  mc->mc_esi;
		if (r & VXSIG_SAVE_EDI)
			emu->cpu.reg[EDI] = mc->mc_edi;
		if (r & VXSIG_SAVE_ESP)
			emu->cpu.reg[ESP] = mc->mc_esp;	// or esp_at_signal ???
		if (r & VXSIG_SAVE_EBP)
			emu->cpu.reg[EBP] = mc->mc_ebp;
		if (r & VXSIG_SAVE_EFLAGS)
			emu->cpu.eflags = mc->mc_eflags;
	}
	r &= ~VXSIG_SAVE_ALL;

	if (r & VXSIG_SAVE_EBX_AS_EIP)
		emu->cpu.eip = mc->mc_ebx;
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
		emu->cpu.traperr = mc->mc_err;
		emu->cpu.trapva = cr2;

		emu->cpu.traperr = mc->mc_err;
		emu->cpu.trapva = cr2;

		memmove(&mc->mc_gs, &emu->trapenv->uc_mcontext.mc_gs, 19*4);
		return 1;
	}

	// The signal handler is confused; so are we.
	return 0;
}


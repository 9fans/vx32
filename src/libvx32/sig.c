#include <sys/signal.h>
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>

#include "vx32.h"
#include "vx32impl.h"

static void sighandler(int signo, siginfo_t *si, void *ucontext)
{
	if (vx32_sighandler(signo, si, ucontext)){
		// Back into the fire.
		return;
	}
	
	// vx32_sighandler thought that vx32 wasn't
	// running or otherwise didn't know what to do.
	// TODO: If there was a handler registered before us, call it?
	// If the signal is not important, ignore it.
	if (signo == SIGVTALRM || signo == SIGALRM)
		return;
	
	// Otherwise, deregister the handler and let the signal
	// happen again -- this time we'll crash and get a core dump.
	vxprint("Unregistering signal handler %d\n", signo);
	signal(signo, SIG_DFL);
}

// vx32_sighandler saves execution context, finds emu,
// and calls vxemu_sighandler.  If we're here, the fs segment
// register pointed at a valid emu, but we still might not have
// been executing actual vx32 translations.
// Trapeip is 0xffffffff if the other segment registers 
// suggest we weren't inside vxrun_setup ... vxrun_cleanup.
int vxemu_sighandler(vxemu *emu, uint32_t trapeip)
{
	if (vx32_debugxlate)
		vxprint("vxemu_sighandler %p %#x\n", emu, trapeip);

	if (emu->cpu_trap == 0)
		return VXSIG_ERROR;
	
	// Not in vx32 code: assume registers saved already and trap.
	if (trapeip == 0xffffffff)
		return VXSIG_TRAP;
	
	if (emu->ininst) {
		// In the middle of translating an instruction,
		// so the registers are already saved.
		// The signal hander knows the exact fault address,
		// which may be a bit after emu->ininst.
		// In fact, the fault address may be one or two
		// instructions past emu->cpu.eip, and a real 
		// processor would not throw the trap until it
		// actually got to the unreadable instruction,
		// but there's not really any harm in trapping now instead.
		return VXSIG_TRAP;
	}

	// Single-stepping?  Use the original trap code.
	if (emu->cpu_trap == VXTRAP_SINGLESTEP && emu->saved_trap){
		emu->cpu_trap = emu->saved_trap;
		emu->saved_trap = 0;
	}

	// In vx32 runtime code (rts.S, run32/64.S), the registers are in flux.
	// Single-step until we can get out; then they'll be safe to look at.
	// This only makes sense if the trap is an external trap like a timer.
	char *eip = (char*)(uintptr_t)trapeip;
	if ((emu->cpu_trap&VXTRAP_CATEGORY) != VXTRAP_CPU){
		// The check for run32/64.S doesn't look for the entire file.
		// Instead it looks for anywhere in the file except
		// vxrun_cleanup.  If we single-step out of vxrun_cleanup,
		// then vx32_sighandler will get a single-step trap when
		// the vx32 segment register doesn't point at emu and 
		// won't know what to do.  If we're in vxrun_cleanup, then
		// all the cpu registers are known to be saved.
		extern char vx_rts_S_start[], vx_rts_S_end[];
		extern char vx_run_S_start[];
		if ((vx_rts_S_start <= eip && eip < vx_rts_S_end)
		||  (vx_run_S_start <= eip && eip < (char*)vxrun_cleanup)){
		SingleStep:
			if(++emu->nsinglestep > 500){
				// Give up: something is wrong.
				vxprint("vx32: single-stepping but stuck\n");
				return VXSIG_ERROR;
			}
			emu->saved_trap = emu->cpu_trap;
			emu->cpu_trap = 0;
			return VXSIG_SINGLESTEP;
		}
	}
	emu->nsinglestep = 0;

	// In translated code buffer?  Find original eip.
	if ((char*)emu->codebuf <= eip && eip < (char*)emu->codefree){
		// Binary search for the translated chunk in which the trap occurred.
		// NB: frags appear in the opposite order as the code itself.
		uint32_t *codetab = emu->codetab;
		unsigned lofrag = 0;
		unsigned hifrag = (uint32_t*)emu->codetop - codetab - 1;
		while (hifrag > lofrag) {
			unsigned midfrag = (lofrag + hifrag) / 2;
			uint32_t mideip = codetab[midfrag];
			if (trapeip >= mideip)
				hifrag = midfrag;
			else
				lofrag = midfrag + 1;
		}
		struct vxfrag *frag = (vxfrag*)(intptr_t)codetab[lofrag];

		// The eip is known to be in the translation buffer,
		// but the buffer contains both fragment headers and
		// fragment code.  Make sure it's not in a fragment header.
		if (trapeip < (uint32_t)(intptr_t)FRAGCODE(frag))
			return VXSIG_ERROR;
		unsigned ofs = trapeip - (uint32_t)(intptr_t)FRAGCODE(frag);

		// The very first instruction in each fragment is the one that
		// restores ebx.  It is not included in the table, because it 
		// doesn't correspond to any original source instruction.
		// If we're there, pretend we're at the first instruction in the
		// fragment but don't save ebx -- it's already saved.
		if (ofs < frag->insn[0].dstofs) {
			emu->cpu.eip = frag->eip + frag->insn[0].srcofs;
			return VXSIG_TRAP | (VXSIG_SAVE_ALL & ~VXSIG_SAVE_EBX);
		}

		// Binary search through this fragment's instruction table
		// for the actual faulting translated instruction,
		// and compute the corresponding EIP in the original vx32 code.
		unsigned ninsn = frag->ninsn;
		unsigned loinsn = 0;
		unsigned hiinsn = ninsn - 1;
		while (hiinsn > loinsn) {
			unsigned midinsn = (loinsn + hiinsn + 1) / 2;
			unsigned midofs = frag->insn[midinsn].dstofs;
			if (ofs >= midofs)
				loinsn = midinsn;
			else
				hiinsn = midinsn - 1;
		}
		struct vxinsn *insn = &frag->insn[loinsn];
		
		if (ofs < insn->dstofs) {
			// How did that happen?
			// We checked for ofs < frag->insn[0].dstofs above.
			assert(0);
		}
		emu->cpu.eip = frag->eip + insn->srcofs;
		
		// At the beginning of a translated instruction (before the
		// translation has begun to execute) all registers are valid.
		if (ofs == insn->dstofs)
			return VXSIG_TRAP | VXSIG_SAVE_ALL;
		
		// But some translations end up being more than one instruction,
		// and we have to handle those specially, if they've executed 
		// only some of the instructions.
		int r;
		switch (insn->itype) {
		default:
			assert(0);

		case VXI_JUMP:
		case VXI_ENDFRAG:
			// Direct jumps don't trash any registers while
			// making their way into vxrun_lookup_backpatch.
			return VXSIG_TRAP | VXSIG_SAVE_ALL;

		case VXI_CALL:
			// Call pushes a return address onto the stack
			// as the first instruction of the translation.
			// We can't just pop it back off, because we would
			// still need to restore the value that got overwritten.
			// The target eip is stored in the word at byte 26 in
			// the translation.
			// Call does not trash any registers on the way 
			// into vxrun_lookup_backpatch.
			emu->cpu.eip = *(uint32_t*)(FRAGCODE(frag)+insn->dstofs+26);
			return VXSIG_TRAP | VXSIG_SAVE_ALL;

		case VXI_JUMPIND:
			// Indirect jumps save ebx as their first instruction,
			// and then they trash it.  Since we're not at the first
			// instruction (see above), it might or might not be 
			// trashed but is definitely already saved.  
			return VXSIG_TRAP | (VXSIG_SAVE_ALL & ~VXSIG_SAVE_EBX);

		case VXI_CALLIND:
			// Indirect call is like indirect jump except that it
			// pushes a return address onto the stack in the
			// instruction that ends at dstlen-5.  Until that point,
			// only ebx has been trashed (but saved) and the stack
			// is unmodified.  At that point, though, the stack is now
			// modified and we must commit the instruction:
			// ebx contains the target eip.
			r = VXSIG_TRAP | (VXSIG_SAVE_ALL & ~VXSIG_SAVE_EBX);
			if (ofs >= insn->dstofs + insn->dstlen - 5)
				r |= VXSIG_SAVE_EBX_AS_EIP;
			return r;
			
		case VXI_RETURN:
		case VXI_RETURN_IMM:;
			// Return is like indirect jump, but we have to treat it
			// as committed once the pop ebx happens.
			// Pop ebx happens at byte 7 of the translation.
			r = VXSIG_TRAP | (VXSIG_SAVE_ALL & ~VXSIG_SAVE_EBX);
			if (ofs - insn->dstofs > 7) {
				r |= VXSIG_SAVE_EBX_AS_EIP;
			
				if (insn->itype == VXI_RETURN_IMM) {
					// Return immediate is like return but if we've committed
					// to popping ebx we also have to commit to popping
					// the immediate number of bytes off the stack.
					// The immediate is a 32-bit word at byte 10 of
					// the translation, but it was originally only 16 bits
					// in the original return instruction, so it's okay to
					// truncate.
					r |= VXSIG_ADD_COUNT_TO_ESP;
					r |= *(uint16_t*)(FRAGCODE(frag)+insn->dstofs+10) << VXSIG_COUNT_SHIFT;
				}
			}
			return r;

		case VXI_TRAP:
			// Traps save eax as their first instruction and then
			// they trash it.  Since we're not at the first instruction
			// (see above), it might or might not be trashed but
			// is definitely already saved.
			return VXSIG_TRAP | (VXSIG_SAVE_ALL & ~VXSIG_SAVE_EAX);
		
		case VXI_LOOP:
		case VXI_LOOPZ:
		case VXI_LOOPNZ:
			// The first instruction in the loop translation decrements ecx.
			// The rest figure out whether to jump.
			// We can back out by re-incrementing ecx.
			// Untested (most loops translate into actual loop instructions).
			return VXSIG_TRAP | VXSIG_SAVE_ALL | VXSIG_INC_ECX;
		}
	}

	// Let's see.
	// The fs segment register pointed at a vxemu, so we're in vxproc_run.
	// The eip is not in rts.S, nor in run32/64.S.
	// The eip is not in a fragment translation.
	// We're not translating an instruction (emu->ininst == nil).
	// That pretty much means we're in some interstitial instruction.
	// Assume the registers are already saved.
	return VXSIG_TRAP;
}

int vx32_siginit(void)
{
	stack_t ss;
	void *stk;
	struct sigaction sa;
	
	// See if there's already an alternate signal stack.
	if (sigaltstack(NULL, &ss) < 0)
		return -1;

	assert(!(ss.ss_flags & SS_ONSTACK));
	if (ss.ss_flags & SS_DISABLE) {
		// Allocate an alternate signal stack.
		ss.ss_size = 64*1024;
		stk = malloc(ss.ss_size);
		if (stk == NULL)
			return -1;
		ss.ss_flags = 0;
		ss.ss_sp = stk;
		if (sigaltstack(&ss, NULL) < 0) {
			free(stk);
			return -1;
		}
	}
	
	// Register our signal handler.
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = (void*)sighandler;
	sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
	if (sigemptyset(&sa.sa_mask) < 0)
		return -1;
	if (sigaction(SIGSEGV, &sa, NULL) < 0)
		return -1;
	if (sigaction(SIGBUS, &sa, NULL) < 0)
		return -1;
	if (sigaction(SIGFPE, &sa, NULL) < 0)
		return -1;
	if (sigaction(SIGVTALRM, &sa, NULL) < 0)
		return -1;
	if (sigaction(SIGTRAP, &sa, NULL) < 0)
		return -1;
	return 0;
}


//
// Assembly-language support code for vx32-to-x86-64 translation
//

#include "libvx32/asm.h"
#include "libvx32/os.h"

	.text

	.globl	vx_run_S_start
vx_run_S_start:

// Set up the segment registers as necessary for the emulation environment.
// Args:
//	rdi: vxemu pointer
//
	.code64

	.globl	vxrun_setup
vxrun_setup:

	// Save the host's normal segment registers.
	movw	%ss,%dx
	movw	%dx,VXEMU_HOST_SS(%rdi)
	movw	%ds,VXEMU_HOST_DS(%rdi)
	movw	%es,VXEMU_HOST_ES(%rdi)
	movw	VSEG,VXEMU_HOST_VS(%rdi)

	// Load the special vxemu segment into VSEG (%fs or %gs).
	movw	VXEMU_EMUSEL(%rdi),VSEG

	ret


// Start running translated vx32 code until something goes wrong -
// usually, until we hit a piece of code that hasn't been translated yet.
//
// Args:
//	rdi: vxemu pointer
//	rsi: translated code entrypoint at which to start running
//
	.p2align 4
	.code64
	.globl	vxrun
vxrun:

	// Save important host registers in x86-64 registers
	// that the 32-bit vx32 environment code can't touch.
	movq	%rdi,%r8	// vxemu pointer argument in r8
	movq	%rbx,%r9 	// callee-saved regs into r9,r10,r11
	movq	%rbp,%r10
	movq	%rsp,%r11

	// Save the translated code entrypoint into the vxemu struct
	// so that we can use it in the far indirect jmp below.
	movl	%esi,VXEMU_RUNPTR(%rdi)

	// Load the vx32 environment's data segment into %ds,%es,%ss.
	// The processor won't actually use the base and offset
	// of this segment until we jump into 32-bit compatibility mode.
	movw	VXEMU_DATASEL(%rdi),%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss

	// Restore vx32 env's eflags register
	movl	VXEMU_EFLAGS(%rdi),%eax
	pushq	%rax
	popfq

	// Load vx32 env's registers
	movl	VXEMU_EAX(%r8),%eax
	movl	VXEMU_ECX(%r8),%ecx
	movl	VXEMU_EDX(%r8),%edx
	// translated code will restore %EBX
	movl	VXEMU_ESP(%r8),%esp
	movl	VXEMU_EBP(%r8),%ebp
	movl	VXEMU_ESI(%r8),%esi
	movl	VXEMU_EDI(%r8),%edi

	// Run translated code
	ljmpl	*VXEMU_RUNPTR(%r8)	// 'ljmpq' doesn't work - gas bug??


// Return from running translated code to the normal host environment.
// Assumes EAX, EBX, ECX, and EDX have already been saved.
// Assumes return code for vxrun is already in eax.
//
	.p2align 4
	.code64
	.globl	vxrun_return
vxrun_return:

	// Save remaining vx32 registers
	movl	%esp,VXEMU_ESP(%r8)
	movl	%ebp,VXEMU_EBP(%r8)
	movl	%esi,VXEMU_ESI(%r8)
	movl	%edi,VXEMU_EDI(%r8)

	// Restore host's callee-saved registers
	movq	%r11,%rsp
	movq	%r10,%rbp
	movq	%r9,%rbx

	// Save vx32 env's eflags register
	pushfq
	popq	%rdx
	movl	%edx,VXEMU_EFLAGS(%r8)

	// Return to caller
	cld	// x86-64 ABI says DF must be cleared
	ret


// Clean up segment registers after running emulated code.
// Args:
//	rdi: vxemu pointer
//
	.code64
	.globl	vxrun_cleanup
vxrun_cleanup:

	// Restore host's original segment registers.
	movw	VXEMU_HOST_SS(%rdi),%ss
	movw	VXEMU_HOST_DS(%rdi),%ds
	movw	VXEMU_HOST_ES(%rdi),%es
	movw	VXEMU_HOST_VS(%rdi),VSEG

	ret
	
// Don't put anything here!
// The signal handler knows that vxrun_cleanup
// is at the bottom of this file.


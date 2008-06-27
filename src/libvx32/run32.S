//
// Assembly-language support code for vx32-to-x86-32 translation
//

#include "asm.h"
#include "os.h"

	.text

	.globl	EXT(vx_run_S_start)
EXT(vx_run_S_start):

// Perform setup necessary for the emulation environment.
// Args:
//	1. vxemu pointer
//
	.globl	EXT(vxrun_setup)
EXT(vxrun_setup):

	// Load vxemu pointer
	movl	4(%esp),%eax

	// Save the host's normal segment registers.
	movw	%ss,%dx
	movw	%dx,VXEMU_HOST_SS(%eax)
	movw	%ds,VXEMU_HOST_DS(%eax)
	movw	%es,VXEMU_HOST_ES(%eax)
	movw	VSEG,VXEMU_HOST_VS(%eax)

	// Load the special vxemu segment into VSEG (%fs or %gs)
	movw	VXEMU_EMUSEL(%eax),VSEG

	ret


// Start running translated vx32 code until something goes wrong -
// usually, until we hit a piece of code that hasn't been translated yet.
//
// Args:
//	1. vxemu pointer
//	2. translated code entrypoint at which to start running
//
	.p2align 4
	.globl	EXT(vxrun)
EXT(vxrun):

	// Save caller's host registers
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	%ebp

	// Fetch translated code entrypoint arg
	movl	(4*4)+4+4(%esp),%ebx

	// Save host esp
	movl	%esp,VSEG:VXEMU_HOST_ESP

	// Restore vx32 env's eflags register
	pushl	VSEG:VXEMU_EFLAGS
	popfl

	// Set up segment registers for vx32 env
	movl	VSEG:VXEMU_DATASEL,%ecx
	movw	%cx,%ds
	movw	%cx,%es
	movw	%cx,%ss

	// Load vx32 env's registers
	movl	VSEG:VXEMU_EAX,%eax
	movl	VSEG:VXEMU_ECX,%ecx
	movl	VSEG:VXEMU_EDX,%edx
	// translated code will restore %EBX
	movl	VSEG:VXEMU_ESP,%esp
	movl	VSEG:VXEMU_EBP,%ebp
	movl	VSEG:VXEMU_ESI,%esi
	movl	VSEG:VXEMU_EDI,%edi

	// Run translated code
	jmp	*%ebx


// Return from running translated code to the normal host environment.
// Assumes EAX, EBX, ECX, and EDX have already been saved.
// Assumes return code for vxrun is already in eax.
//
	.p2align 4
	.globl	EXT(vxrun_return)
EXT(vxrun_return):

	// Save remaining vx32 registers
	movl	%esp,VSEG:VXEMU_ESP
	movl	%ebp,VSEG:VXEMU_EBP
	movl	%esi,VSEG:VXEMU_ESI
	movl	%edi,VSEG:VXEMU_EDI

	// Restore host's normal segment registers
	movw	VSEG:VXEMU_HOST_DS,%ds
	movw	VSEG:VXEMU_HOST_ES,%es
	movw	VSEG:VXEMU_HOST_SS,%ss

	// Switch back to host's stack
	movl	VSEG:VXEMU_HOST_ESP,%esp

	// Save vx32 env's eflags register
	pushfl
	popl	VSEG:VXEMU_EFLAGS

	// Restore host's callee-save registers and return to caller
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%ebx
	ret


// Clean up after a stint of running VX code.
// Args:
//	1. vxemu pointer
//
	.globl	EXT(vxrun_cleanup)
EXT(vxrun_cleanup):

	// Load vxemu pointer
	movl	4(%esp),%eax

	// Restore host's FS/GS register.
	// (DS/ES/SS were already restored by vxrun_return.)
	movw	VXEMU_HOST_VS(%eax),VSEG

	ret
	
// Don't put anything here!
// The signal handler knows that vxrun_cleanup
// is at the bottom of this file.

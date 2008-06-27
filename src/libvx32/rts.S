//
// Assembly-language runtime support for translated vx32 code.
//

#include "asm.h"
#include "os.h"


// The method we use to get back out to host code differs for 32/64-bit hosts.
#ifdef __i386
#ifdef __APPLE__
#define RETURN	jmp	vxrun_return_stub
	.section __IMPORT,__jump_table,symbol_stubs,self_modifying_code+pure_instructions,5
vxrun_return_stub:
	.indirect_symbol EXT(vxrun_return)
	hlt ; hlt ; hlt ; hlt ; hlt
#else	// i386, not APPLE
#define RETURN	jmp	EXT(vxrun_return)
#endif	// i386, not APPLE
#else	// x86-64
#define	RETURN	ljmpl	*VSEG:VXEMU_RETPTR
#endif


	.text

	// All the following runtime support code is 32-bit
	// (even on 64-bit hosts).
	.code32

	.globl	EXT(vx_rts_S_start)
EXT(vx_rts_S_start):


// Look up a vx32 EIP and jump to the corresponding translated code entrypoint,
// then back-patching the calling instruction to point to the translated code.
//
// Called from trampolines embedded out-of-line in translated code.
// The inline jmp/jcc instruction looks like this:
//		jmp	2f		// ALWAYS uses a rel32, not a rel8
//	1:
//
// A translated call instruction is the same, except prefixed by:
//		pushl	$<return_eip>
//
// Then the out-of-line trampoline code looks like this:
//	2:	movl	$3f,VSEG:vx_jmpinfo
//		jmp	vx_lookup_backpatch
//	3:	.long	dest_eip
//		.long	3b-1b		// offset of jmp inst to backpatch
//
	.globl	EXT(vxrun_lookup_backpatch)
EXT(vxrun_lookup_backpatch):

	// Save registers we'll need, and load the jump information.
	movl	%edx,VSEG:VXEMU_EDX
	movl	VSEG:VXEMU_JMPINFO,%edx	// edx: jmpinfo pointer
	movl	%ebx,VSEG:VXEMU_EBX
	movl	VSEG:(%edx),%ebx	// ebx: target eip
	movl	%ecx,VSEG:VXEMU_ECX
	movl	%ebx,%ecx		// ecx: target eip
	movl	%eax,VSEG:VXEMU_EAX

	// Save condition codes (except for DF, which we don't modify here).
	// We use LAHF and SAHF instead of pushf/popf because the latter
	// would require us to reload the stack segment, which is slow.
	// But unfortunately the OF is not in the low 8 bits of EFLAGS
	// covered by LAHF/SAHF, so we have to save that flag separately.
	lahf		// Copy low 8 bits of EFLAGS into AH
	seto	%al	// Set %al to 0x00 or 0xff according to OF

	// Hash the vx32 target EIP into ecx.
	shrl	$10,%ecx
	addl	%ebx,%ecx
	shrl	$10,%ecx
	subl	%ebx,%ecx

1:	// Look up the appropriate hash table entry
	andl	VSEG:VXEMU_ETABMASK,%ecx
	cmpl	VSEG:VXEMU_ETAB(,%ecx,8),%ebx
	jne	2f

	// Found the correct entry!
	// Get the translated code entrypoint into ebx.
	movl	VSEG:VXEMU_ETAB+4(,%ecx,8),%ebx

	// Backpatch the original jmp instruction with this translated target.
	// %edx still points to the jmpinfo structure.
	movl	VSEG:4(%edx),%edx	// find end of original jmp insn
	lea	7(%ebx),%ecx		// skip target's load-ebx prolog
	subl	%edx,%ecx		// form 32-bit relative jmp target
	subl	VSEG:VXEMU_EMUPTR,%edx	// form vxemu-relative jmp insn offset
	movl	%ecx,VSEG:-4(%edx)	// patch jmp insn

	// Restore condition codes.
	shrb	$1,%al	// Restore overflow flag
	sahf		// Restore low 8 bits of EFLAGS

	// Restore other registers
	movl	VSEG:VXEMU_EAX,%eax
	movl	VSEG:VXEMU_ECX,%ecx
	movl	VSEG:VXEMU_EDX,%edx

	// Jump to appropriate translated code entrypoint.
	// The translated code will restore ebx before doing anything else.
	jmp	*%ebx

2:	// Not the correct entry - walk forward until we find
	// the correct entry or the first empty one.
	incl	%ecx
	cmpl	$-1,VSEG:VXEMU_ETAB-8(,%ecx,8)	// XX NULLSRCEIP
	jne	1b

3:	// No correct entry - drop back to C to translate more code.

	// Save EIP that we were trying to jump to
	movl	%ebx,VSEG:VXEMU_EIP

	// Restore condition codes
	shrb	$1,%al	// Restore overflow flag
	sahf		// Restore low 8 bits of EFLAGS

	// Indicate that we're returning due to a translation miss
	movl	$0,%eax		// Careful not to trash condition codes

	// Return to host code.
	RETURN



// Look up a vx32 EIP and jump to the corresponding translated code entrypoint,
// without backpatching.  Used to handle indirect jmps and calls.
//
// The generated code for an indirect jump looks basically like this:
//		movl	%ebx,VSEG:VXEMU_EBX
//		movl	<indirect_ea>,%ebx
//		jmp	vx_lookup_indirect
//
// The generated code for an indirect call is as follows:
//		movl	%ebx,VSEG:VXEMU_EBX
//		movl	<indirect_ea>,%ebx
//		pushl	$<return_eip>
//		jmp	vx_lookup_indirect
//
// Finally, the code generated for a RET instruction looks like this:
//		movl	%ebx,VSEG:VXEMU_EBX
//		popl	%ebx
//		jmp	vx_lookup_indirect
//
	.p2align 4
	.globl	EXT(vxrun_lookup_indirect)
EXT(vxrun_lookup_indirect):

	// Save more registers we'll need
	movl	%eax,VSEG:VXEMU_EAX
	movl	%ecx,VSEG:VXEMU_ECX

	// Save condition codes (except for DF, which we don't modify here).
	// We use LAHF and SAHF instead of pushf/popf because the latter
	// would require us to reload the stack segment, which is slow.
	// But unfortunately the OF is not in the low 8 bits of EFLAGS
	// covered by LAHF/SAHF, so we have to save that flag separately.
	lahf		// Copy low 8 bits of EFLAGS into AH
	seto	%al	// Set %al to 0x00 or 0xff according to OF

	// Hash the vx32 EIP into ecx.
	movl	%ebx,%ecx
	shrl	$10,%ecx
	addl	%ebx,%ecx
	shrl	$10,%ecx
	subl	%ebx,%ecx

1:	// Look up the appropriate hash table entry
	andl	VSEG:VXEMU_ETABMASK,%ecx
	cmpl	VSEG:VXEMU_ETAB(,%ecx,8),%ebx
	jne	2f

	// Found the correct entry!
	// Get the translated code entrypoint into ebx.
	movl	VSEG:VXEMU_ETAB+4(,%ecx,8),%ebx

	// Restore condition codes.
	shrb	$1,%al	// Restore overflow flag
	sahf		// Restore low 8 bits of EFLAGS

	// Restore other registers
	movl	VSEG:VXEMU_EAX,%eax
	movl	VSEG:VXEMU_ECX,%ecx

	// Jump to appropriate translated code entrypoint.
	// The translated code will restore ebx before doing anything else.
	jmp	*%ebx

2:	// Not the correct entry - walk forward until we find
	// the correct entry or the first empty one.
	incl	%ecx
	cmpl	$-1,VSEG:VXEMU_ETAB-8(,%ecx,8)	// XX NULLSRCEIP
	jne	1b

3:	// No correct entry - drop back to C to translate more code.
	movl	%edx,VSEG:VXEMU_EDX

	// Save EIP that we were trying to jump to
	movl	%ebx,VSEG:VXEMU_EIP

	// Restore condition codes
	shrb	$1,%al	// Restore overflow flag
	sahf		// Restore low 8 bits of EFLAGS

	// Indicate that we're returning due to a translation miss
	movl	$0,%eax		// Careful not to trash condition codes

	// Jump to host code.
	RETURN


// Return from running translated code, generating a VX trap.
// Assumes the environment's EAX has already been saved
// and EAX now contains the trap code to return.
	.p2align 4
	.globl	EXT(vxrun_gentrap)
EXT(vxrun_gentrap):
gentrap:
	movl	%ebx,VSEG:VXEMU_EBX
	movl	%ecx,VSEG:VXEMU_ECX
	movl	%edx,VSEG:VXEMU_EDX
	// vxrun_return will save the others

	// Besides returning it, also record trap number in emu struct.
	movl	%eax,VSEG:VXEMU_TRAPNO

	RETURN


// Special "pseudo-fragment" entrypoint used as the dstip
// for unused entries in the entrypoint hash table.
// The corresponding srcip in unused entries is -1, an invalid VX address.
// This way we can keep the unused entry check
// off the critical "cache hit" path for collision-free entrypoint lookups:
// if VX code does jump to address -1, it'll just wind up here.
	.globl	EXT(vxrun_nullfrag)
EXT(vxrun_nullfrag):
	movl	VSEG:VXEMU_EBX,%ebx	// standard fragment prolog
	movl	%eax,VSEG:VXEMU_EAX	// standard trap generation code
	movl	$0x106,%eax		// VXTRAP_INVALID
	jmp	gentrap


	.globl	EXT(vx_rts_S_end)
EXT(vx_rts_S_end):

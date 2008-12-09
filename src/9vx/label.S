#ifdef __APPLE__
#define EXT(s) _ ## s
#else
#define EXT(s) s
#endif

#ifdef i386

.globl EXT(setlabel)
EXT(setlabel):
	movl	4(%esp), %eax
	
	movl %ebp, 0(%eax)
	movl %ebx, 8(%eax)
	movl %esi, 16(%eax)
	movl %edi, 24(%eax)
	movl %esp, 32(%eax)

	movl 0(%esp), %ecx
	movl %ecx, 40(%eax)
	
	movl $0, %eax
	ret

.globl EXT(gotolabel)
EXT(gotolabel):
	movl	4(%esp), %eax
	
	movl 0(%eax), %ebp
	movl 8(%eax), %ebx
	movl 16(%eax), %esi
	movl 24(%eax), %edi
	movl 32(%eax), %esp
	
	movl 40(%eax), %ecx
	movl %ecx, 0(%esp)
	
	movl $1, %eax
	ret

#else

// first param is %rdi
// need to save %rbp, %rbx, %rsp, %rip, %r12-%r15.
// %rsi and %rdi are not callee-save, unlike i386.

.globl EXT(setlabel)
EXT(setlabel):
	movq %rbp, 0(%rdi)
	movq %rbx, 8(%rdi)
	// 16(%rdi) is saved si (not used)
	// 24(%rdi) is saved di (not used)
	movq %rsp, 32(%rdi)
	movq 0(%rsp), %rax
	movq %rax, 40(%rdi)	// %rip
	movq %r12, 48(%rdi)
	movq %r13, 56(%rdi)
	movq %r14, 64(%rdi)
	movq %r15, 72(%rdi)
	movq $0, %rax
	ret

.globl EXT(gotolabel)
EXT(gotolabel):
	movq 0(%rdi), %rbp
	movq 8(%rdi), %rbx
	movq 32(%rdi), %rsp
	movq 40(%rdi), %rax
	movq %rax, 0(%rsp)	// %rip
	movq 48(%rdi), %r12
	movq 56(%rdi), %r13
	movq 64(%rdi), %r14
	movq 72(%rdi), %r15
	movq $1, %rax
	cld
	ret

#endif

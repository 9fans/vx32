// Context routines for Linux

// vx32_getcontext(struct mcontext_t *ss);  
// see /usr/include/sys/ucontext.h for layout.
// notice that we don't set the floating-point state.
// notice also that unlike the linux getcontext,
// we *do* copy the segment registers

#ifdef i386

.globl vx32_getcontext
vx32_getcontext:
	movl 4(%esp), %eax
	
	movl %ecx, 40(%eax)
	movl $0, %ecx
	movw %gs, %cx
	movl %ecx, 0(%eax)
	movw %fs, %cx
	movl %ecx, 4(%eax)
	movw %es, %cx
	movl %ecx, 8(%eax)
	movw %ds, %cx
	movl %ecx, 12(%eax)
	movl %edi, 16(%eax)
	movl %esi, 20(%eax)
	movl %ebp, 24(%eax)
	/* 28(%eax) is esp */
	movl %ebx, 32(%eax)
	movl %edx, 36(%eax)
	/* 40(%eax) is ecx, saved above */
	movl $1, 44(%eax)	/* eax */
	movl $0, 48(%eax)	/* trapno */
	movl $0, 52(%eax)	/* err */
	/* 56(%eax) is eip */
	movw %cs, %cx
	movl %ecx, 60(%eax)
	pushf
	movl 0(%esp), %ecx
	popf
	movl %ecx, 64(%eax)
	/* 68(%eax) is another esp */
	movw %ss, %cx
	movl %ecx, 72(%eax)
	
	movl 0(%esp), %ecx		/* %eip */
	movl %ecx, 56(%eax)
	leal 4(%esp), %ecx		/* %esp */
	movl %ecx, 28(%eax)
	movl %ecx, 68(%eax)
	
	movl 40(%eax), %ecx		/* restore %ecx */
	movl $0, %eax
	ret

#else	/* x86-64 */

.globl vx32_getcontext
vx32_getcontext:
	// mcontext_t pointer is in %rdi
	movq %r8, 0(%rdi)
	movq %r9, 8(%rdi)
	movq %r10, 16(%rdi)
	movq %r11, 24(%rdi)
	movq %r12, 32(%rdi)
	movq %r13, 40(%rdi)
	movq %r14, 48(%rdi)
	movq %r15, 56(%rdi)
	movq %rdi, 64(%rdi)
	movq %rsi, 72(%rdi)
	movq %rbp, 80(%rdi)
	movq %rbx, 88(%rdi)
	movq %rdx, 96(%rdi)
	movq $1, 104(%rdi)	/* %rax */
	movq %rcx, 112(%rdi)
	/* 120(%rdi) is rsp */
	leaq 8(%rsp), %rax
	movq %rax, 120(%rdi)
	/* 128(%rdi) is rip */
	movq 0(%rsp), %rax
	movq %rax, 128(%rdi)
	/* 136(%rdi) is eflags */
	pushf
	popq %rax
	movq %rax, 136(%rdi)
	/* 144(%rdi) is cs, gs, fs, pad */
	xorq %rax, %rax
	movw %fs, %ax
	shlq $16, %rax
	movw %gs, %ax
	shlq $16, %rax
	movw %cs, %ax
	movq %rax, 144(%rdi)
	/* 152(%rdi) is err */
	movq $0, 152(%rdi)
	/* 160(%rdi) is trapno */
	movq $0, 160(%rdi)
	/* 168(%rdi) is oldmask */
	movq $0, 168(%rdi)
	/* 176(%rdi) is cr2 */
	movq $0, 176(%rdi)
	movq $0, %rax
	ret

#endif

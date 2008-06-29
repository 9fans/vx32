// Context routines for Linux

// vx32_getcontext(struct mcontext_t *ss);  
// see /usr/include/sys/ucontext.h for layout.
// notice that we don't set the floating-point state.
// notice also that unlike the linux getcontext,
// we *do* copy the segment registers

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
	pushfl
	popl 64(%eax)
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


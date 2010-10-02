// Context routines for OS X

// vx32_getcontext(struct i386_thread_state *ss);  
// see /usr/include/mach/i386/thread_status.h for layout.
// not traditional intel layout!

.globl _vx32_getcontext
_vx32_getcontext:
	movl 4(%esp), %eax
	
	movl $1, 0(%eax) /* %eax */
	movl %ebx, 4(%eax)
	movl %ecx, 8(%eax)
	movl %edx, 12(%eax)
	movl %edi, 16(%eax)
	movl %esi, 20(%eax)
	movl %ebp, 24(%eax)
	/* 28(%eax) is esp */
	movl %ss, 32(%eax)
	pushfl
	popl 36(%eax)
	/* 40(%eax) is eip */
	movl %cs, 44(%eax)
	movl %ds, 48(%eax)
	movl %es, 52(%eax)
	movl %fs, 56(%eax)
	movl %gs, 60(%eax)
	
	movl 0(%esp), %ecx		/* %eip */
	movl %ecx, 40(%eax)
	leal 4(%esp), %ecx		/* %esp */
	movl %ecx, 28(%eax)
	
	movl 8(%eax), %ecx		/* restore %ecx */
	movl $0, %eax
	ret


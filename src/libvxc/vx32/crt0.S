	.text
	.globl	_start
_start:
	// argc is on stack
	popl	%ebx
	// argv *is* stack (%esp)
	// environ starts at argv+argc+1
	leal 4(%esp,%ebx,4), %eax
	movl %eax, environ
	// call main
	pushl %esp
	pushl %ebx
	call	main

	pushl	%eax
	call	exit

1:	jmp	1b

	.data
	.globl environ
.p2align 2
environ:
	.long 0




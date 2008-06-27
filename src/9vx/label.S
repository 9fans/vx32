#ifdef __APPLE__
#define EXT(s) _ ## s
#else
#define EXT(s) s
#endif

.globl EXT(setlabel)
EXT(setlabel):
	movl	4(%esp), %eax
	
	movl %ebp, 0(%eax)
	movl %ebx, 4(%eax)
	movl %esi, 8(%eax)
	movl %edi, 12(%eax)
	movl %esp, 16(%eax)

	movl 0(%esp), %ecx
	movl %ecx, 20(%eax)
	
	movl $0, %eax
	ret

.globl EXT(gotolabel)
EXT(gotolabel):
	movl	4(%esp), %eax
	
	movl 0(%eax), %ebp
	movl 4(%eax), %ebx
	movl 8(%eax), %esi
	movl 12(%eax), %edi
	movl 16(%eax), %esp
	
	movl 20(%eax), %ecx
	movl %ecx, 0(%esp)
	
	movl $1, %eax
	ret

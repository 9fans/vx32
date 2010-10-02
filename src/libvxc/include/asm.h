#ifndef _ASM_H_
#define _ASM_H_

#define ENTRY(x)	.text; .p2align 2,0x90; \
			.globl x; .type x,@function; x:

#endif	// _ASM_H_

/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <asm.h>

ENTRY(__ieee754_scalbf)
	flds	8(%esp)
	flds	4(%esp)
	fscale
	ret

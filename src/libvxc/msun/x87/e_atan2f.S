/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <asm.h>

ENTRY(__ieee754_atan2f)
	flds	4(%esp)
	flds	8(%esp)
	fpatan
	ret

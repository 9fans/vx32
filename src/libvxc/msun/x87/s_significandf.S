/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <asm.h>

ENTRY(significandf)
	flds	4(%esp)
	fxtract
	fstp	%st(1)
	ret

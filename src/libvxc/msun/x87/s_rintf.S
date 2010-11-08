/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <asm.h>

ENTRY(rintf)
	flds	4(%esp)
	frndint
	ret

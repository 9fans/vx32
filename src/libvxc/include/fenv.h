/*-
 * Copyright (c) 2004 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_FENV_H_
#define	_FENV_H_


// The floating-point environment for VX32 is just the 32-bit MXCSR register.
typedef unsigned fenv_t;
typedef	unsigned fexcept_t;

/* Exception flags */
#define	FE_INVALID	0x01
#define	FE_DENORMAL	0x02
#define	FE_DIVBYZERO	0x04
#define	FE_OVERFLOW	0x08
#define	FE_UNDERFLOW	0x10
#define	FE_INEXACT	0x20
#define	FE_ALL_EXCEPT	(FE_DIVBYZERO | FE_DENORMAL | FE_INEXACT | \
			 FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW)

/* Rounding modes */
#define	FE_TONEAREST	0x0000
#define	FE_DOWNWARD	0x2000
#define	FE_UPWARD	0x4000
#define	FE_TOWARDZERO	0x6000
#define	_ROUND_MASK	(FE_TONEAREST | FE_DOWNWARD | \
			 FE_UPWARD | FE_TOWARDZERO)

/* Default floating-point environment for MXCSR */
#define	FE_DFL_ENV	0x0x1F80


#define	__ldmxcsr(__csr)	__asm __volatile("ldmxcsr %0" : : "m" (__csr))
#define	__stmxcsr(__csr)	__asm __volatile("stmxcsr %0" : "=m" (*(__csr)))

static __inline int
feclearexcept(int __excepts)
{
	fenv_t __env;

	__stmxcsr(&__env);
	__env &= ~__excepts;
	__ldmxcsr(__env);
	return (0);
}

static __inline int
fegetexceptflag(fexcept_t *__flagp, int __excepts)
{
	unsigned __mxcsr;

	__stmxcsr(&__mxcsr);
	*__flagp = __mxcsr & __excepts;
	return (0);
}

int fesetexceptflag(const fexcept_t *__flagp, int __excepts);
int feraiseexcept(int __excepts);

static __inline int
fetestexcept(int __excepts)
{
	unsigned __mxcsr;

	__stmxcsr(&__mxcsr);
	return (__mxcsr & __excepts);
}

static __inline int
fegetround(void)
{
	unsigned __mxcsr;

	__stmxcsr(&__mxcsr);
	return (__mxcsr & _ROUND_MASK);
}

static __inline int
fesetround(int __round)
{
	unsigned __mxcsr;

	if (__round & ~_ROUND_MASK)
		return (-1);

	__stmxcsr(&__mxcsr);
	__mxcsr &= ~_ROUND_MASK;
	__mxcsr |= __round;
	__ldmxcsr(__mxcsr);

	return (0);
}

int feholdexcept(fenv_t *__envp);

static __inline int
fegetenv(fenv_t *__envp)
{
	__stmxcsr(__envp);
	return (0);
}

static __inline int
fesetenv(const fenv_t *__envp)
{
	__ldmxcsr(*__envp);
	return (0);
}

int feupdateenv(const fenv_t *__envp);


#endif	/* !_FENV_H_ */

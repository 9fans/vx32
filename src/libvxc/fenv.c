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
 *
 * $FreeBSD: src/lib/msun/amd64/fenv.c,v 1.2 2004/06/11 02:35:19 das Exp $
 */

#include <fenv.h>

int
fesetexceptflag(const fexcept_t *flagp, int excepts)
{
	unsigned __mxcsr;

	__stmxcsr(&__mxcsr);
	__mxcsr &= ~excepts;
	__mxcsr |= *flagp & excepts;
	__ldmxcsr(__mxcsr);

	return (0);
}

int
feraiseexcept(int excepts)
{
	fexcept_t ex = excepts;

	fesetexceptflag(&ex, excepts);

	// The DIVSS instruction can generate all SSE2 exceptions,
	// which hopefully means it will check all exception flags.
	// asm volatile("divss %0,%0" : : "x" (1.0));

	return (0);
}

int
feholdexcept(fenv_t *envp)
{
	int mxcsr;

	__stmxcsr(&mxcsr);
	*envp = mxcsr;
	mxcsr &= ~FE_ALL_EXCEPT;
	mxcsr |= FE_ALL_EXCEPT << 7;
	__ldmxcsr(mxcsr);
	return (0);
}

int
feupdateenv(const fenv_t *envp)
{
	int mxcsr;

	__stmxcsr(&mxcsr);
	fesetenv(envp);
	feraiseexcept(mxcsr & FE_ALL_EXCEPT);
	return (0);
}


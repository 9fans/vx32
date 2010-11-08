/*-
 * Copyright (c) 2003 Mike Barcroft <mike@FreeBSD.org>
 * Copyright (c) 2002 David Schultz <dschultz@uclink.Berkeley.EDU>
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
 * $FreeBSD: src/lib/libc/gen/isinf.c,v 1.2 2004/07/10 15:52:26 marcel Exp $
 */

#include <math.h>

union IEEEf2bits {
	float	f;
	struct {
		unsigned int	man	:23;
		unsigned int	exp	:8;
		unsigned int	sign	:1;
	} bits;
};

#define	DBL_MANH_SIZE	20
#define	DBL_MANL_SIZE	32

union IEEEd2bits {
	double	d;
	struct {
		unsigned int	manl	:32;
		unsigned int	manh	:20;
		unsigned int	exp	:11;
		unsigned int	sign	:1;
	} bits;
};

union IEEEl2bits {
	long double	e;
	struct {
		unsigned int	manl	:32;
		unsigned int	manh	:32;
		unsigned int	exp	:15;
		unsigned int	sign	:1;
		unsigned int	junk	:16;
	} bits;
};

#define	mask_nbit_l(u)	((u).bits.manh &= 0x7fffffff)

#define	LDBL_MANH_SIZE	32
#define	LDBL_MANL_SIZE	32

#define	LDBL_TO_ARRAY32(u, a) do {			\
	(a)[0] = (uint32_t)(u).bits.manl;		\
	(a)[1] = (uint32_t)(u).bits.manh;		\
} while(0)

/*
 * XXX These routines belong in libm, but they must remain in libc for
 *     binary compat until we can bump libm's major version number.
 */

#undef isinf
int
isinf(double d)
{
	union IEEEd2bits u;

	u.d = d;
	return (u.bits.exp == 2047 && u.bits.manl == 0 && u.bits.manh == 0);
}

int
__isinff(float f)
{
	union IEEEf2bits u;

	u.f = f;
	return (u.bits.exp == 255 && u.bits.man == 0);
}

int
__isinfl(long double e)
{
	union IEEEl2bits u;

	u.e = e;
	mask_nbit_l(u);
#ifndef __alpha__
	return (u.bits.exp == 32767 && u.bits.manl == 0 && u.bits.manh == 0);
#else
	return (u.bits.exp == 2047 && u.bits.manl == 0 && u.bits.manh == 0);
#endif
}

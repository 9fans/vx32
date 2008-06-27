/*
 * The authors of this software are Rob Pike and Ken Thompson,
 * with contributions from Mike Burrows and Sean Dorward.
 *
 *     Copyright (c) 2002-2006 by Lucent Technologies.
 *     Portions Copyright (c) 2004 Google Inc.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES 
 * NOR GOOGLE INC MAKE ANY REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING 
 * THE MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include "u.h"
#include "utf.h"
#include "fmt.h"

#define PLAN9PORT /* Get Plan 9 verbs */

/*
 * compiler directive on Plan 9
 */
#ifndef USED
#define USED(x) if(x);else
#endif

/*
 * nil cannot be ((void*)0) on ANSI C,
 * because it is used for function pointers
 */
#undef	nil
#define	nil	0

#undef	nelem
#define	nelem(x)	(sizeof (x)/sizeof (x)[0])

/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */

/*
 * dofmt -- format to a buffer
 * the number of characters formatted is returned,
 * or -1 if there was an error.
 * if the buffer is ever filled, flush is called.
 * it should reset the buffer and return whether formatting should continue.
 */

typedef int (*Fmts)(Fmt*);

typedef struct Quoteinfo Quoteinfo;
struct Quoteinfo
{
	int	quoted;		/* if set, string must be quoted */
	int	nrunesin;	/* number of input runes that can be accepted */
	int	nbytesin;	/* number of input bytes that can be accepted */
	int	nrunesout;	/* number of runes that will be generated */
	int	nbytesout;	/* number of bytes that will be generated */
};

/* Edit .+1,/^$/ |cfn |grep -v static | grep __ */
double       __Inf(int sign);
double       __NaN(void);
int          __badfmt(Fmt *f);
int          __charfmt(Fmt *f);
int          __countfmt(Fmt *f);
int          __efgfmt(Fmt *fmt);
int          __errfmt(Fmt *f);
int          __flagfmt(Fmt *f);
int          __fmtFdFlush(Fmt *f);
int          __fmtcpy(Fmt *f, const void *vm, int n, int sz);
void*        __fmtdispatch(Fmt *f, void *fmt, int isrunes);
void *       __fmtflush(Fmt *f, void *t, int len);
void         __fmtlock(void);
int          __fmtpad(Fmt *f, int n);
double       __fmtpow10(int n);
int          __fmtrcpy(Fmt *f, const void *vm, int n);
void         __fmtunlock(void);
int          __ifmt(Fmt *f);
int          __isInf(double d, int sign);
int          __isNaN(double d);
int          __needsep(int*, char**);
int          __needsquotes(char *s, int *quotelenp);
int          __percentfmt(Fmt *f);
void         __quotesetup(char *s, Rune *r, int nin, int nout, Quoteinfo *q, int sharp, int runesout);
int          __quotestrfmt(int runesin, Fmt *f);
int          __rfmtpad(Fmt *f, int n);
int          __runefmt(Fmt *f);
int          __runeneedsquotes(Rune *r, int *quotelenp);
int          __runesfmt(Fmt *f);
int          __strfmt(Fmt *f);

#define FMTCHAR(f, t, s, c)\
	do{\
	if(t + 1 > (char*)s){\
		t = (char*)__fmtflush(f, t, 1);\
		if(t != nil)\
			s = (char*)f->stop;\
		else\
			return -1;\
	}\
	*t++ = c;\
	}while(0)

#define FMTRCHAR(f, t, s, c)\
	do{\
	if(t + 1 > (Rune*)s){\
		t = (Rune*)__fmtflush(f, t, sizeof(Rune));\
		if(t != nil)\
			s = (Rune*)f->stop;\
		else\
			return -1;\
	}\
	*t++ = c;\
	}while(0)

#define FMTRUNE(f, t, s, r)\
	do{\
	Rune _rune;\
	int _runelen;\
	if(t + UTFmax > (char*)s && t + (_runelen = runelen(r)) > (char*)s){\
		t = (char*)__fmtflush(f, t, _runelen);\
		if(t != nil)\
			s = (char*)f->stop;\
		else\
			return -1;\
	}\
	if(r < Runeself)\
		*t++ = r;\
	else{\
		_rune = r;\
		t += runetochar(t, &_rune);\
	}\
	}while(0)

#ifdef va_copy
#	define VA_COPY(a,b) va_copy(a,b)
#	define VA_END(a) va_end(a)
#else
#	define VA_COPY(a,b) (a) = (b)
#	define VA_END(a)
#endif


/* ---------- end preamble -------- */

/* -------------- charstod.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

/*
 * Reads a floating-point number by interpreting successive characters
 * returned by (*f)(vp).  The last call it makes to f terminates the
 * scan, so is not a character in the number.  It may therefore be
 * necessary to back up the input stream up one byte after calling charstod.
 */

double
fmtcharstod(int(*f)(void*), void *vp)
{
	double num, dem;
	int neg, eneg, dig, exp, c;

	num = 0;
	neg = 0;
	dig = 0;
	exp = 0;
	eneg = 0;

	c = (*f)(vp);
	while(c == ' ' || c == '\t')
		c = (*f)(vp);
	if(c == '-' || c == '+'){
		if(c == '-')
			neg = 1;
		c = (*f)(vp);
	}
	while(c >= '0' && c <= '9'){
		num = num*10 + c-'0';
		c = (*f)(vp);
	}
	if(c == '.')
		c = (*f)(vp);
	while(c >= '0' && c <= '9'){
		num = num*10 + c-'0';
		dig++;
		c = (*f)(vp);
	}
	if(c == 'e' || c == 'E'){
		c = (*f)(vp);
		if(c == '-' || c == '+'){
			if(c == '-'){
				dig = -dig;
				eneg = 1;
			}
			c = (*f)(vp);
		}
		while(c >= '0' && c <= '9'){
			exp = exp*10 + c-'0';
			c = (*f)(vp);
		}
	}
	exp -= dig;
	if(exp < 0){
		exp = -exp;
		eneg = !eneg;
	}
	dem = __fmtpow10(exp);
	if(eneg)
		num /= dem;
	else
		num *= dem;
	if(neg)
		return -num;
	return num;
}
/* -------------- dofmt.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
/* Copyright (c) 2004 Google Inc.; see LICENSE */

// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

/* format the output into f->to and return the number of characters fmted  */
int
dofmt(Fmt *f, char *fmt)
{
	Rune rune, *rt, *rs;
	int r;
	char *t, *s;
	int n, nfmt;

	nfmt = f->nfmt;
	for(;;){
		if(f->runes){
			rt = (Rune*)f->to;
			rs = (Rune*)f->stop;
			while((r = *(uchar*)fmt) && r != '%'){
				if(r < Runeself)
					fmt++;
				else{
					fmt += chartorune(&rune, fmt);
					r = rune;
				}
				FMTRCHAR(f, rt, rs, r);
			}
			fmt++;
			f->nfmt += rt - (Rune *)f->to;
			f->to = rt;
			if(!r)
				return f->nfmt - nfmt;
			f->stop = rs;
		}else{
			t = (char*)f->to;
			s = (char*)f->stop;
			while((r = *(uchar*)fmt) && r != '%'){
				if(r < Runeself){
					FMTCHAR(f, t, s, r);
					fmt++;
				}else{
					n = chartorune(&rune, fmt);
					if(t + n > s){
						t = (char*)__fmtflush(f, t, n);
						if(t != nil)
							s = (char*)f->stop;
						else
							return -1;
					}
					while(n--)
						*t++ = *fmt++;
				}
			}
			fmt++;
			f->nfmt += t - (char *)f->to;
			f->to = t;
			if(!r)
				return f->nfmt - nfmt;
			f->stop = s;
		}

		fmt = (char*)__fmtdispatch(f, fmt, 0);
		if(fmt == nil)
			return -1;
	}
}

void *
__fmtflush(Fmt *f, void *t, int len)
{
	if(f->runes)
		f->nfmt += (Rune*)t - (Rune*)f->to;
	else
		f->nfmt += (char*)t - (char *)f->to;
	f->to = t;
	if(f->flush == 0 || (*f->flush)(f) == 0 || (char*)f->to + len > (char*)f->stop){
		f->stop = f->to;
		return nil;
	}
	return f->to;
}

/*
 * put a formatted block of memory sz bytes long of n runes into the output buffer,
 * left/right justified in a field of at least f->width characters (if FmtWidth is set)
 */
int
__fmtpad(Fmt *f, int n)
{
	char *t, *s;
	int i;

	t = (char*)f->to;
	s = (char*)f->stop;
	for(i = 0; i < n; i++)
		FMTCHAR(f, t, s, ' ');
	f->nfmt += t - (char *)f->to;
	f->to = t;
	return 0;
}

int
__rfmtpad(Fmt *f, int n)
{
	Rune *t, *s;
	int i;

	t = (Rune*)f->to;
	s = (Rune*)f->stop;
	for(i = 0; i < n; i++)
		FMTRCHAR(f, t, s, ' ');
	f->nfmt += t - (Rune *)f->to;
	f->to = t;
	return 0;
}

int
__fmtcpy(Fmt *f, const void *vm, int n, int sz)
{
	Rune *rt, *rs, r;
	char *t, *s, *m, *me;
	ulong fl;
	int nc, w;

	m = (char*)vm;
	me = m + sz;
	fl = f->flags;
	w = 0;
	if(fl & FmtWidth)
		w = f->width;
	if((fl & FmtPrec) && n > f->prec)
		n = f->prec;
	if(f->runes){
		if(!(fl & FmtLeft) && __rfmtpad(f, w - n) < 0)
			return -1;
		rt = (Rune*)f->to;
		rs = (Rune*)f->stop;
		for(nc = n; nc > 0; nc--){
			r = *(uchar*)m;
			if(r < Runeself)
				m++;
			else if((me - m) >= UTFmax || fullrune(m, me-m))
				m += chartorune(&r, m);
			else
				break;
			FMTRCHAR(f, rt, rs, r);
		}
		f->nfmt += rt - (Rune *)f->to;
		f->to = rt;
		if(fl & FmtLeft && __rfmtpad(f, w - n) < 0)
			return -1;
	}else{
		if(!(fl & FmtLeft) && __fmtpad(f, w - n) < 0)
			return -1;
		t = (char*)f->to;
		s = (char*)f->stop;
		for(nc = n; nc > 0; nc--){
			r = *(uchar*)m;
			if(r < Runeself)
				m++;
			else if((me - m) >= UTFmax || fullrune(m, me-m))
				m += chartorune(&r, m);
			else
				break;
			FMTRUNE(f, t, s, r);
		}
		f->nfmt += t - (char *)f->to;
		f->to = t;
		if(fl & FmtLeft && __fmtpad(f, w - n) < 0)
			return -1;
	}
	return 0;
}

int
__fmtrcpy(Fmt *f, const void *vm, int n)
{
	Rune r, *m, *me, *rt, *rs;
	char *t, *s;
	ulong fl;
	int w;

	m = (Rune*)vm;
	fl = f->flags;
	w = 0;
	if(fl & FmtWidth)
		w = f->width;
	if((fl & FmtPrec) && n > f->prec)
		n = f->prec;
	if(f->runes){
		if(!(fl & FmtLeft) && __rfmtpad(f, w - n) < 0)
			return -1;
		rt = (Rune*)f->to;
		rs = (Rune*)f->stop;
		for(me = m + n; m < me; m++)
			FMTRCHAR(f, rt, rs, *m);
		f->nfmt += rt - (Rune *)f->to;
		f->to = rt;
		if(fl & FmtLeft && __rfmtpad(f, w - n) < 0)
			return -1;
	}else{
		if(!(fl & FmtLeft) && __fmtpad(f, w - n) < 0)
			return -1;
		t = (char*)f->to;
		s = (char*)f->stop;
		for(me = m + n; m < me; m++){
			r = *m;
			FMTRUNE(f, t, s, r);
		}
		f->nfmt += t - (char *)f->to;
		f->to = t;
		if(fl & FmtLeft && __fmtpad(f, w - n) < 0)
			return -1;
	}
	return 0;
}

/* fmt out one character */
int
__charfmt(Fmt *f)
{
	char x[1];

	x[0] = va_arg(f->args, int);
	f->prec = 1;
	return __fmtcpy(f, (const char*)x, 1, 1);
}

/* fmt out one rune */
int
__runefmt(Fmt *f)
{
	Rune x[1];

	x[0] = va_arg(f->args, int);
	return __fmtrcpy(f, (const void*)x, 1);
}

/* public helper routine: fmt out a null terminated string already in hand */
int
fmtstrcpy(Fmt *f, char *s)
{
	int i, j;

	if(!s)
		return __fmtcpy(f, "<nil>", 5, 5);
	/* if precision is specified, make sure we don't wander off the end */
	if(f->flags & FmtPrec){
#ifdef PLAN9PORT
		Rune r;
		i = 0;
		for(j=0; j<f->prec && s[i]; j++)
			i += chartorune(&r, s+i);
#else
		/* ANSI requires precision in bytes, not Runes */
		for(i=0; i<f->prec; i++)
			if(s[i] == 0)
				break;
		j = utfnlen(s, i);	/* won't print partial at end */
#endif
		return __fmtcpy(f, s, j, i);
	}
	return __fmtcpy(f, s, utflen(s), strlen(s));
}

/* fmt out a null terminated utf string */
int
__strfmt(Fmt *f)
{
	char *s;

	s = va_arg(f->args, char *);
	return fmtstrcpy(f, s);
}

/* public helper routine: fmt out a null terminated rune string already in hand */
int
fmtrunestrcpy(Fmt *f, Rune *s)
{
	Rune *e;
	int n, p;

	if(!s)
		return __fmtcpy(f, "<nil>", 5, 5);
	/* if precision is specified, make sure we don't wander off the end */
	if(f->flags & FmtPrec){
		p = f->prec;
		for(n = 0; n < p; n++)
			if(s[n] == 0)
				break;
	}else{
		for(e = s; *e; e++)
			;
		n = e - s;
	}
	return __fmtrcpy(f, s, n);
}

/* fmt out a null terminated rune string */
int
__runesfmt(Fmt *f)
{
	Rune *s;

	s = va_arg(f->args, Rune *);
	return fmtrunestrcpy(f, s);
}

/* fmt a % */
int
__percentfmt(Fmt *f)
{
	Rune x[1];

	x[0] = f->r;
	f->prec = 1;
	return __fmtrcpy(f, (const void*)x, 1);
}

/* fmt an integer */
int
__ifmt(Fmt *f)
{
	char buf[140], *p, *conv;
	/* 140: for 64 bits of binary + 3-byte sep every 4 digits */
	uvlong vu;
	ulong u;
	int neg, base, i, n, fl, w, isv;
	int ndig, len, excess, bytelen;
	char *grouping;
	char *thousands;

	neg = 0;
	fl = f->flags;
	isv = 0;
	vu = 0;
	u = 0;
#ifndef PLAN9PORT
	/*
	 * Unsigned verbs for ANSI C
	 */
	switch(f->r){
	case 'o':
	case 'p':
	case 'u':
	case 'x':
	case 'X':
		fl |= FmtUnsigned;
		fl &= ~(FmtSign|FmtSpace);
		break;
	}
#endif
	if(f->r == 'p'){
		u = (ulong)va_arg(f->args, void*);
		f->r = 'x';
		fl |= FmtUnsigned;
	}else if(fl & FmtVLong){
		isv = 1;
		if(fl & FmtUnsigned)
			vu = va_arg(f->args, uvlong);
		else
			vu = va_arg(f->args, vlong);
	}else if(fl & FmtLong){
		if(fl & FmtUnsigned)
			u = va_arg(f->args, ulong);
		else
			u = va_arg(f->args, long);
	}else if(fl & FmtByte){
		if(fl & FmtUnsigned)
			u = (uchar)va_arg(f->args, int);
		else
			u = (char)va_arg(f->args, int);
	}else if(fl & FmtShort){
		if(fl & FmtUnsigned)
			u = (ushort)va_arg(f->args, int);
		else
			u = (short)va_arg(f->args, int);
	}else{
		if(fl & FmtUnsigned)
			u = va_arg(f->args, uint);
		else
			u = va_arg(f->args, int);
	}
	conv = "0123456789abcdef";
	grouping = "\4";	/* for hex, octal etc. (undefined by spec but nice) */
	thousands = f->thousands;
	switch(f->r){
	case 'd':
	case 'i':
	case 'u':
		base = 10;
		grouping = f->grouping;
		break;
	case 'X':
		conv = "0123456789ABCDEF";
		/* fall through */
	case 'x':
		base = 16;
		thousands = ":";
		break;
	case 'b':
		base = 2;
		thousands = ":";
		break;
	case 'o':
		base = 8;
		break;
	default:
		return -1;
	}
	if(!(fl & FmtUnsigned)){
		if(isv && (vlong)vu < 0){
			vu = -(vlong)vu;
			neg = 1;
		}else if(!isv && (long)u < 0){
			u = -(long)u;
			neg = 1;
		}
	}
	p = buf + sizeof buf - 1;
	n = 0;	/* in runes */
	excess = 0;	/* number of bytes > number runes */
	ndig = 0;
	len = utflen(thousands);
	bytelen = strlen(thousands);
	if(isv){
		while(vu){
			i = vu % base;
			vu /= base;
			if((fl & FmtComma) && n % 4 == 3){
				*p-- = ',';
				n++;
			}
			if((fl & FmtApost) && __needsep(&ndig, &grouping)){
				n += len;
				excess += bytelen - len;
				p -= bytelen;
				memmove(p+1, thousands, bytelen);
			}
			*p-- = conv[i];
			n++;
		}
	}else{
		while(u){
			i = u % base;
			u /= base;
			if((fl & FmtComma) && n % 4 == 3){
				*p-- = ',';
				n++;
			}
			if((fl & FmtApost) && __needsep(&ndig, &grouping)){
				n += len;
				excess += bytelen - len;
				p -= bytelen;
				memmove(p+1, thousands, bytelen);
			}
			*p-- = conv[i];
			n++;
		}
	}
	if(n == 0){
		/*
		 * "The result of converting a zero value with
		 * a precision of zero is no characters."  - ANSI
		 *
		 * "For o conversion, # increases the precision, if and only if
		 * necessary, to force the first digit of the result to be a zero
		 * (if the value and precision are both 0, a single 0 is printed)." - ANSI
		 */
		if(!(fl & FmtPrec) || f->prec != 0 || (f->r == 'o' && (fl & FmtSharp))){
			*p-- = '0';
			n = 1;
			if(fl & FmtApost)
				__needsep(&ndig, &grouping);
		}
		
		/*
		 * Zero values don't get 0x.
		 */
		if(f->r == 'x' || f->r == 'X')
			fl &= ~FmtSharp;
	}
	for(w = f->prec; n < w && p > buf+3; n++){
		if((fl & FmtApost) && __needsep(&ndig, &grouping)){
			n += len;
			excess += bytelen - len;
			p -= bytelen;
			memmove(p+1, thousands, bytelen);
		}
		*p-- = '0';
	}
	if(neg || (fl & (FmtSign|FmtSpace)))
		n++;
	if(fl & FmtSharp){
		if(base == 16)
			n += 2;
		else if(base == 8){
			if(p[1] == '0')
				fl &= ~FmtSharp;
			else
				n++;
		}
	}
	if((fl & FmtZero) && !(fl & (FmtLeft|FmtPrec))){
		w = 0;
		if(fl & FmtWidth)
			w = f->width;
		for(; n < w && p > buf+3; n++){
			if((fl & FmtApost) && __needsep(&ndig, &grouping)){
				n += len;
				excess += bytelen - len;
				p -= bytelen;
				memmove(p+1, thousands, bytelen);
			}
			*p-- = '0';
		}
		f->flags &= ~FmtWidth;
	}
	if(fl & FmtSharp){
		if(base == 16)
			*p-- = f->r;
		if(base == 16 || base == 8)
			*p-- = '0';
	}
	if(neg)
		*p-- = '-';
	else if(fl & FmtSign)
		*p-- = '+';
	else if(fl & FmtSpace)
		*p-- = ' ';
	f->flags &= ~FmtPrec;
	return __fmtcpy(f, p + 1, n, n + excess);
}

int
__countfmt(Fmt *f)
{
	void *p;
	ulong fl;

	fl = f->flags;
	p = va_arg(f->args, void*);
	if(fl & FmtVLong){
		*(vlong*)p = f->nfmt;
	}else if(fl & FmtLong){
		*(long*)p = f->nfmt;
	}else if(fl & FmtByte){
		*(char*)p = f->nfmt;
	}else if(fl & FmtShort){
		*(short*)p = f->nfmt;
	}else{
		*(int*)p = f->nfmt;
	}
	return 0;
}

int
__flagfmt(Fmt *f)
{
	switch(f->r){
	case ',':
		f->flags |= FmtComma;
		break;
	case '-':
		f->flags |= FmtLeft;
		break;
	case '+':
		f->flags |= FmtSign;
		break;
	case '#':
		f->flags |= FmtSharp;
		break;
	case '\'':
		f->flags |= FmtApost;
		break;
	case ' ':
		f->flags |= FmtSpace;
		break;
	case 'u':
		f->flags |= FmtUnsigned;
		break;
	case 'h':
		if(f->flags & FmtShort)
			f->flags |= FmtByte;
		f->flags |= FmtShort;
		break;
	case 'L':
		f->flags |= FmtLDouble;
		break;
	case 'l':
		if(f->flags & FmtLong)
			f->flags |= FmtVLong;
		f->flags |= FmtLong;
		break;
	}
	return 1;
}

/* default error format */
int
__badfmt(Fmt *f)
{
	char x[3];

	x[0] = '%';
	x[1] = f->r;
	x[2] = '%';
	f->prec = 3;
	__fmtcpy(f, (const void*)x, 3, 3);
	return 0;
}
/* -------------- fltfmt.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdio.h>
// #include <math.h>
// #include <float.h>
// #include <string.h>
// #include <stdlib.h>
// #include <errno.h>
// #include <stdarg.h>
// #include <fmt.h>
// #include <assert.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"
// #include "nan.h"

enum
{
	FDIGIT	= 30,
	FDEFLT	= 6,
	NSIGNIF	= 17
};

/*
 * first few powers of 10, enough for about 1/2 of the
 * total space for doubles.
 */
static double pows10[] =
{
	  1e0,   1e1,   1e2,   1e3,   1e4,   1e5,   1e6,   1e7,   1e8,   1e9,  
	 1e10,  1e11,  1e12,  1e13,  1e14,  1e15,  1e16,  1e17,  1e18,  1e19,  
	 1e20,  1e21,  1e22,  1e23,  1e24,  1e25,  1e26,  1e27,  1e28,  1e29,  
	 1e30,  1e31,  1e32,  1e33,  1e34,  1e35,  1e36,  1e37,  1e38,  1e39,  
	 1e40,  1e41,  1e42,  1e43,  1e44,  1e45,  1e46,  1e47,  1e48,  1e49,  
	 1e50,  1e51,  1e52,  1e53,  1e54,  1e55,  1e56,  1e57,  1e58,  1e59,  
	 1e60,  1e61,  1e62,  1e63,  1e64,  1e65,  1e66,  1e67,  1e68,  1e69,  
	 1e70,  1e71,  1e72,  1e73,  1e74,  1e75,  1e76,  1e77,  1e78,  1e79,  
	 1e80,  1e81,  1e82,  1e83,  1e84,  1e85,  1e86,  1e87,  1e88,  1e89,  
	 1e90,  1e91,  1e92,  1e93,  1e94,  1e95,  1e96,  1e97,  1e98,  1e99,  
	1e100, 1e101, 1e102, 1e103, 1e104, 1e105, 1e106, 1e107, 1e108, 1e109, 
	1e110, 1e111, 1e112, 1e113, 1e114, 1e115, 1e116, 1e117, 1e118, 1e119, 
	1e120, 1e121, 1e122, 1e123, 1e124, 1e125, 1e126, 1e127, 1e128, 1e129, 
	1e130, 1e131, 1e132, 1e133, 1e134, 1e135, 1e136, 1e137, 1e138, 1e139, 
	1e140, 1e141, 1e142, 1e143, 1e144, 1e145, 1e146, 1e147, 1e148, 1e149, 
	1e150, 1e151, 1e152, 1e153, 1e154, 1e155, 1e156, 1e157, 1e158, 1e159, 
};
#define	npows10 ((int)(sizeof(pows10)/sizeof(pows10[0])))
#define	pow10(x)  fmtpow10(x)

static double
pow10(int n)
{
	double d;
	int neg;

	neg = 0;
	if(n < 0){
		if(n < DBL_MIN_10_EXP)
			return 0.;
		neg = 1;
		n = -n;
	}else if(n > DBL_MAX_10_EXP)
		return HUGE_VAL;

	if(n < npows10)
		d = pows10[n];
	else{
		d = pows10[npows10-1];
		for(;;){
			n -= npows10 - 1;
			if(n < npows10){
				d *= pows10[n];
				break;
			}
			d *= pows10[npows10 - 1];
		}
	}
	if(neg)
		return 1./d;
	return d;
}

/*
 * add 1 to the decimal integer string a of length n.
 * if 99999 overflows into 10000, return 1 to tell caller
 * to move the virtual decimal point.
 */
static int
xadd1(char *a, int n)
{
	char *b;
	int c;

	if(n < 0 || n > NSIGNIF)
		return 0;
	for(b = a+n-1; b >= a; b--) {
		c = *b + 1;
		if(c <= '9') {
			*b = c;
			return 0;
		}
		*b = '0';
	}
	/*
	 * need to overflow adding digit.
	 * shift number down and insert 1 at beginning.
	 * decimal is known to be 0s or we wouldn't
	 * have gotten this far.  (e.g., 99999+1 => 00000)
	 */
	a[0] = '1';
	return 1;
}

/*
 * subtract 1 from the decimal integer string a.
 * if 10000 underflows into 09999, make it 99999
 * and return 1 to tell caller to move the virtual 
 * decimal point.  this way, xsub1 is inverse of xadd1.
 */
static int
xsub1(char *a, int n)
{
	char *b;
	int c;

	if(n < 0 || n > NSIGNIF)
		return 0;
	for(b = a+n-1; b >= a; b--) {
		c = *b - 1;
		if(c >= '0') {
			if(c == '0' && b == a) {
				/*
				 * just zeroed the top digit; shift everyone up.
				 * decimal is known to be 9s or we wouldn't
				 * have gotten this far.  (e.g., 10000-1 => 09999)
				 */
				*b = '9';
				return 1;
			}
			*b = c;
			return 0;
		}
		*b = '9';
	}
	/*
	 * can't get here.  the number a is always normalized
	 * so that it has a nonzero first digit.
	 */
	abort();
}

/*
 * format exponent like sprintf(p, "e%+02d", e)
 */
static void
xfmtexp(char *p, int e, int ucase)
{
	char se[9];
	int i;

	*p++ = ucase ? 'E' : 'e';
	if(e < 0) {
		*p++ = '-';
		e = -e;
	} else
		*p++ = '+';
	i = 0;
	while(e) {
		se[i++] = e % 10 + '0';
		e /= 10;
	}
	while(i < 2)
		se[i++] = '0';
	while(i > 0)
		*p++ = se[--i];
	*p++ = '\0';
}

/*
 * compute decimal integer m, exp such that:
 *	f = m*10^exp
 *	m is as short as possible with losing exactness
 * assumes special cases (NaN, +Inf, -Inf) have been handled.
 */
static void
xdtoa(double f, char *s, int *exp, int *neg, int *ns)
{
	int c, d, e2, e, ee, i, ndigit, oerrno;
	char tmp[NSIGNIF+10];
	double g;

	oerrno = errno; /* in case strtod smashes errno */

	/*
	 * make f non-negative.
	 */
	*neg = 0;
	if(f < 0) {
		f = -f;
		*neg = 1;
	}

	/*
	 * must handle zero specially.
	 */
	if(f == 0){
		*exp = 0;
		s[0] = '0';
		s[1] = '\0';
		*ns = 1;
		return;
	}
		
	/*
	 * find g,e such that f = g*10^e.
	 * guess 10-exponent using 2-exponent, then fine tune.
	 */
	frexp(f, &e2);
	e = (int)(e2 * .301029995664);
	g = f * pow10(-e);
	while(g < 1) {
		e--;
		g = f * pow10(-e);
	}
	while(g >= 10) {
		e++;
		g = f * pow10(-e);
	}

	/*
	 * convert NSIGNIF digits as a first approximation.
	 */
	for(i=0; i<NSIGNIF; i++) {
		d = (int)g;
		s[i] = d+'0';
		g = (g-d) * 10;
	}
	s[i] = 0;

	/*
	 * adjust e because s is 314159... not 3.14159...
	 */
	e -= NSIGNIF-1;
	xfmtexp(s+NSIGNIF, e, 0);

	/*
	 * adjust conversion until strtod(s) == f exactly.
	 */
	for(i=0; i<10; i++) {
		g = strtod(s, nil);
		if(f > g) {
			if(xadd1(s, NSIGNIF)) {
				/* gained a digit */
				e--;
				xfmtexp(s+NSIGNIF, e, 0);
			}
			continue;
		}
		if(f < g) {
			if(xsub1(s, NSIGNIF)) {
				/* lost a digit */
				e++;
				xfmtexp(s+NSIGNIF, e, 0);
			}
			continue;
		}
		break;
	}

	/*
	 * play with the decimal to try to simplify.
	 */

	/*
	 * bump last few digits up to 9 if we can
	 */
	for(i=NSIGNIF-1; i>=NSIGNIF-3; i--) {
		c = s[i];
		if(c != '9') {
			s[i] = '9';
			g = strtod(s, nil);
			if(g != f) {
				s[i] = c;
				break;
			}
		}
	}

	/*
	 * add 1 in hopes of turning 9s to 0s
	 */
	if(s[NSIGNIF-1] == '9') {
		strcpy(tmp, s);
		ee = e;
		if(xadd1(tmp, NSIGNIF)) {
			ee--;
			xfmtexp(tmp+NSIGNIF, ee, 0);
		}
		g = strtod(tmp, nil);
		if(g == f) {
			strcpy(s, tmp);
			e = ee;
		}
	}
	
	/*
	 * bump last few digits down to 0 as we can.
	 */
	for(i=NSIGNIF-1; i>=NSIGNIF-3; i--) {
		c = s[i];
		if(c != '0') {
			s[i] = '0';
			g = strtod(s, nil);
			if(g != f) {
				s[i] = c;
				break;
			}
		}
	}

	/*
	 * remove trailing zeros.
	 */
	ndigit = NSIGNIF;
	while(ndigit > 1 && s[ndigit-1] == '0'){
		e++;
		--ndigit;
	}
	s[ndigit] = 0;
	*exp = e;
	*ns = ndigit;
	errno = oerrno;
}

#ifdef PLAN9PORT
static char *special[] = { "NaN", "NaN", "+Inf", "+Inf", "-Inf", "-Inf" };
#else
static char *special[] = { "nan", "NAN", "inf", "INF", "-inf", "-INF" };
#endif

int
__efgfmt(Fmt *fmt)
{
	char buf[NSIGNIF+10], *dot, *digits, *p, *s, suf[10], *t;
	double f;
	int c, chr, dotwid, e, exp, fl, ndigits, neg, newndigits;
	int pad, point, prec, realchr, sign, sufwid, ucase, wid, z1, z2;
	Rune r, *rs, *rt;
	
	f = va_arg(fmt->args, double);
	
	/* 
	 * extract formatting flags
	 */
	fl = fmt->flags;
	fmt->flags = 0;
	prec = FDEFLT;
	if(fl & FmtPrec)
		prec = fmt->prec;
	chr = fmt->r;
	ucase = 0;
	switch(chr) {
	case 'A':
	case 'E':
	case 'F':
	case 'G':
		chr += 'a'-'A';
		ucase = 1;
		break;
	}

	/*
	 * pick off special numbers.
	 */
	if(__isNaN(f)) {
		s = special[0+ucase];
	special:
		fmt->flags = fl & (FmtWidth|FmtLeft);
		return __fmtcpy(fmt, s, strlen(s), strlen(s));
	}
	if(__isInf(f, 1)) {
		s = special[2+ucase];
		goto special;
	}
	if(__isInf(f, -1)) {
		s = special[4+ucase];
		goto special;
	}

	/*
	 * get exact representation.
	 */
	digits = buf;
	xdtoa(f, digits, &exp, &neg, &ndigits);

	/*
	 * get locale's decimal point.
	 */
	dot = fmt->decimal;
	if(dot == nil)
		dot = ".";
	dotwid = utflen(dot);

	/*
	 * now the formatting fun begins.
	 * compute parameters for actual fmt:
	 *
	 *	pad: number of spaces to insert before/after field.
	 *	z1: number of zeros to insert before digits
	 *	z2: number of zeros to insert after digits
	 *	point: number of digits to print before decimal point
	 *	ndigits: number of digits to use from digits[]
	 *	suf: trailing suffix, like "e-5"
	 */
	realchr = chr;
	switch(chr){
	case 'g':
		/*
		 * convert to at most prec significant digits. (prec=0 means 1)
		 */
		if(prec == 0)
			prec = 1;
		if(ndigits > prec) {
			if(digits[prec] >= '5' && xadd1(digits, prec))
				exp++;
			exp += ndigits-prec;
			ndigits = prec;
		}
		
		/*
		 * extra rules for %g (implemented below):
		 *	trailing zeros removed after decimal unless FmtSharp.
		 *	decimal point only if digit follows.
		 */

		/* fall through to %e */
	default:
	case 'e':
		/* 
		 * one significant digit before decimal, no leading zeros.
		 */
		point = 1;
		z1 = 0;
		
		/*
		 * decimal point is after ndigits digits right now.
		 * slide to be after first.
		 */
		e  = exp + (ndigits-1);

		/*
		 * if this is %g, check exponent and convert prec
		 */
		if(realchr == 'g') {
			if(-4 <= e && e < prec)
				goto casef;
			prec--;	/* one digit before decimal; rest after */
		}

		/*
		 * compute trailing zero padding or truncate digits.
		 */
		if(1+prec >= ndigits)
			z2 = 1+prec - ndigits;
		else {
			/*
			 * truncate digits
			 */
			assert(realchr != 'g');
			newndigits = 1+prec;
			if(digits[newndigits] >= '5' && xadd1(digits, newndigits)) {
				/*
				 * had 999e4, now have 100e5
				 */
				e++;
			}
			ndigits = newndigits;
			z2 = 0;
		}
		xfmtexp(suf, e, ucase);
		sufwid = strlen(suf);
		break;

	casef:
	case 'f':
		/*
		 * determine where digits go with respect to decimal point
		 */
		if(ndigits+exp > 0) {
			point = ndigits+exp;
			z1 = 0;
		} else {
			point = 1;
			z1 = 1 + -(ndigits+exp);
		}

		/*
		 * %g specifies prec = number of significant digits
		 * convert to number of digits after decimal point
		 */
		if(realchr == 'g')
			prec += z1 - point;

		/*
		 * compute trailing zero padding or truncate digits.
		 */
		if(point+prec >= z1+ndigits)
			z2 = point+prec - (z1+ndigits);
		else {
			/*
			 * truncate digits
			 */
			assert(realchr != 'g');
			newndigits = point+prec - z1;
			if(newndigits < 0) {
				z1 += newndigits;
				newndigits = 0;
			} else if(newndigits == 0) {
				/* perhaps round up */
				if(digits[0] >= '5'){
					digits[0] = '1';
					newndigits = 1;
					goto newdigit;
				}
			} else if(digits[newndigits] >= '5' && xadd1(digits, newndigits)) {
				/*
				 * digits was 999, is now 100; make it 1000
				 */
				digits[newndigits++] = '0';
			newdigit:
				/*
				 * account for new digit
				 */
				if(z1)	/* 0.099 => 0.100 or 0.99 => 1.00*/
					z1--;
				else	/* 9.99 => 10.00 */
					point++;
			}
			z2 = 0;
			ndigits = newndigits;
		}	
		sufwid = 0;
		break;
	}
	
	/*
	 * if %g is given without FmtSharp, remove trailing zeros.
	 * must do after truncation, so that e.g. print %.3g 1.001
	 * produces 1, not 1.00.  sorry, but them's the rules.
	 */
	if(realchr == 'g' && !(fl & FmtSharp)) {
		if(z1+ndigits+z2 >= point) {
			if(z1+ndigits < point)
				z2 = point - (z1+ndigits);
			else{
				z2 = 0;
				while(z1+ndigits > point && digits[ndigits-1] == '0')
					ndigits--;
			}
		}
	}

	/*
	 * compute width of all digits and decimal point and suffix if any
	 */
	wid = z1+ndigits+z2;
	if(wid > point)
		wid += dotwid;
	else if(wid == point){
		if(fl & FmtSharp)
			wid += dotwid;
		else
			point++;	/* do not print any decimal point */
	}
	wid += sufwid;

	/*
	 * determine sign
	 */
	sign = 0;
	if(neg)
		sign = '-';
	else if(fl & FmtSign)
		sign = '+';
	else if(fl & FmtSpace)
		sign = ' ';
	if(sign)
		wid++;

	/*
	 * compute padding
	 */
	pad = 0;
	if((fl & FmtWidth) && fmt->width > wid)
		pad = fmt->width - wid;
	if(pad && !(fl & FmtLeft) && (fl & FmtZero)){
		z1 += pad;
		point += pad;
		pad = 0;
	}

	/*
	 * format the actual field.  too bad about doing this twice.
	 */
	if(fmt->runes){
		if(pad && !(fl & FmtLeft) && __rfmtpad(fmt, pad) < 0)
			return -1;
		rt = (Rune*)fmt->to;
		rs = (Rune*)fmt->stop;
		if(sign)
			FMTRCHAR(fmt, rt, rs, sign);
		while(z1>0 || ndigits>0 || z2>0) {
			if(z1 > 0){
				z1--;
				c = '0';
			}else if(ndigits > 0){
				ndigits--;
				c = *digits++;
			}else{
				z2--;
				c = '0';
			}
			FMTRCHAR(fmt, rt, rs, c);
			if(--point == 0) {
				for(p = dot; *p; ){
					p += chartorune(&r, p);
					FMTRCHAR(fmt, rt, rs, r);
				}
			}
		}
		fmt->nfmt += rt - (Rune*)fmt->to;
		fmt->to = rt;
		if(sufwid && __fmtcpy(fmt, suf, sufwid, sufwid) < 0)
			return -1;
		if(pad && (fl & FmtLeft) && __rfmtpad(fmt, pad) < 0)
			return -1;
	}else{
		if(pad && !(fl & FmtLeft) && __fmtpad(fmt, pad) < 0)
			return -1;
		t = (char*)fmt->to;
		s = (char*)fmt->stop;
		if(sign)
			FMTCHAR(fmt, t, s, sign);
		while(z1>0 || ndigits>0 || z2>0) {
			if(z1 > 0){
				z1--;
				c = '0';
			}else if(ndigits > 0){
				ndigits--;
				c = *digits++;
			}else{
				z2--;
				c = '0';
			}
			FMTCHAR(fmt, t, s, c);
			if(--point == 0)
				for(p=dot; *p; p++)
					FMTCHAR(fmt, t, s, *p);
		}
		fmt->nfmt += t - (char*)fmt->to;
		fmt->to = t;
		if(sufwid && __fmtcpy(fmt, suf, sufwid, sufwid) < 0)
			return -1;
		if(pad && (fl & FmtLeft) && __fmtpad(fmt, pad) < 0)
			return -1;
	}
	return 0;
}

/* -------------- fmt.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

enum
{
	Maxfmt = 64
};

typedef struct Convfmt Convfmt;
struct Convfmt
{
	int	c;
	volatile	Fmts	fmt;	/* for spin lock in fmtfmt; avoids race due to write order */
};

static struct
{
	/* lock by calling __fmtlock, __fmtunlock */
	int	nfmt;
	Convfmt	fmt[Maxfmt];
} fmtalloc;

static Convfmt knownfmt[] = {
	{ ' ',	__flagfmt },
	{ '#',	__flagfmt },
	{ '%',	__percentfmt },
	{ '\'',	__flagfmt },
	{ '+',	__flagfmt },
	{ ',',	__flagfmt },
	{ '-',	__flagfmt },
	{ 'C',	__runefmt },	/* Plan 9 addition */
	{ 'E',	__efgfmt },
#ifndef PLAN9PORT
	{ 'F',	__efgfmt },	/* ANSI only */
#endif
	{ 'G',	__efgfmt },
#ifndef PLAN9PORT
	{ 'L',	__flagfmt },	/* ANSI only */
#endif
	{ 'S',	__runesfmt },	/* Plan 9 addition */
	{ 'X',	__ifmt },
	{ 'b',	__ifmt },		/* Plan 9 addition */
	{ 'c',	__charfmt },
	{ 'd',	__ifmt },
	{ 'e',	__efgfmt },
	{ 'f',	__efgfmt },
	{ 'g',	__efgfmt },
	{ 'h',	__flagfmt },
#ifndef PLAN9PORT
	{ 'i',	__ifmt },		/* ANSI only */
#endif
	{ 'l',	__flagfmt },
	{ 'n',	__countfmt },
	{ 'o',	__ifmt },
	{ 'p',	__ifmt },
	{ 'r',	__errfmt },
	{ 's',	__strfmt },
#ifdef PLAN9PORT
	{ 'u',	__flagfmt },
#else
	{ 'u',	__ifmt },
#endif
	{ 'x',	__ifmt },
	{ 0,	nil }
};


int	(*fmtdoquote)(int);

/*
 * __fmtlock() must be set
 */
static int
__fmtinstall(int c, Fmts f)
{
	Convfmt *p, *ep;

	if(c<=0 || c>=65536)
		return -1;
	if(!f)
		f = __badfmt;

	ep = &fmtalloc.fmt[fmtalloc.nfmt];
	for(p=fmtalloc.fmt; p<ep; p++)
		if(p->c == c)
			break;

	if(p == &fmtalloc.fmt[Maxfmt])
		return -1;

	p->fmt = f;
	if(p == ep){	/* installing a new format character */
		fmtalloc.nfmt++;
		p->c = c;
	}

	return 0;
}

int
fmtinstall(int c, int (*f)(Fmt*))
{
	int ret;

	__fmtlock();
	ret = __fmtinstall(c, f);
	__fmtunlock();
	return ret;
}

static Fmts
fmtfmt(int c)
{
	Convfmt *p, *ep;

	ep = &fmtalloc.fmt[fmtalloc.nfmt];
	for(p=fmtalloc.fmt; p<ep; p++)
		if(p->c == c){
			while(p->fmt == nil)	/* loop until value is updated */
				;
			return p->fmt;
		}

	/* is this a predefined format char? */
	__fmtlock();
	for(p=knownfmt; p->c; p++)
		if(p->c == c){
			__fmtinstall(p->c, p->fmt);
			__fmtunlock();
			return p->fmt;
		}
	__fmtunlock();

	return __badfmt;
}

void*
__fmtdispatch(Fmt *f, void *fmt, int isrunes)
{
	Rune rune, r;
	int i, n;

	f->flags = 0;
	f->width = f->prec = 0;

	for(;;){
		if(isrunes){
			r = *(Rune*)fmt;
			fmt = (Rune*)fmt + 1;
		}else{
			fmt = (char*)fmt + chartorune(&rune, (char*)fmt);
			r = rune;
		}
		f->r = r;
		switch(r){
		case '\0':
			return nil;
		case '.':
			f->flags |= FmtWidth|FmtPrec;
			continue;
		case '0':
			if(!(f->flags & FmtWidth)){
				f->flags |= FmtZero;
				continue;
			}
			/* fall through */
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			i = 0;
			while(r >= '0' && r <= '9'){
				i = i * 10 + r - '0';
				if(isrunes){
					r = *(Rune*)fmt;
					fmt = (Rune*)fmt + 1;
				}else{
					r = *(char*)fmt;
					fmt = (char*)fmt + 1;
				}
			}
			if(isrunes)
				fmt = (Rune*)fmt - 1;
			else
				fmt = (char*)fmt - 1;
		numflag:
			if(f->flags & FmtWidth){
				f->flags |= FmtPrec;
				f->prec = i;
			}else{
				f->flags |= FmtWidth;
				f->width = i;
			}
			continue;
		case '*':
			i = va_arg(f->args, int);
			if(i < 0){
				/*
				 * negative precision =>
				 * ignore the precision.
				 */
				if(f->flags & FmtPrec){
					f->flags &= ~FmtPrec;
					f->prec = 0;
					continue;
				}
				i = -i;
				f->flags |= FmtLeft;
			}
			goto numflag;
		}
		n = (*fmtfmt(r))(f);
		if(n < 0)
			return nil;
		if(n == 0)
			return fmt;
	}
}
/* -------------- fmtfd.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

/*
 * public routine for final flush of a formatting buffer
 * to a file descriptor; returns total char count.
 */
int
fmtfdflush(Fmt *f)
{
	if(__fmtFdFlush(f) <= 0)
		return -1;
	return f->nfmt;
}

/*
 * initialize an output buffer for buffered printing
 */
int
fmtfdinit(Fmt *f, int fd, char *buf, int size)
{
	f->runes = 0;
	f->start = buf;
	f->to = buf;
	f->stop = buf + size;
	f->flush = __fmtFdFlush;
	f->farg = (void*)(uintptr_t)fd;
	f->flags = 0;
	f->nfmt = 0;
	fmtlocaleinit(f, nil, nil, nil);
	return 0;
}
/* -------------- fmtfdflush.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <unistd.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

/*
 * generic routine for flushing a formatting buffer
 * to a file descriptor
 */
int
__fmtFdFlush(Fmt *f)
{
	int n;

	n = (char*)f->to - (char*)f->start;
	if(n && write((uintptr)f->farg, f->start, n) != n)
		return 0;
	f->to = f->start;
	return 1;
}
/* -------------- fmtlocale.c --------------- */
/* Copyright (c) 2004 Google Inc.; see LICENSE */

// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

/*
 * Fill in the internationalization stuff in the State structure.
 * For nil arguments, provide the sensible defaults:
 *	decimal is a period
 *	thousands separator is a comma
 *	thousands are marked every three digits
 */
void
fmtlocaleinit(Fmt *f, char *decimal, char *thousands, char *grouping)
{
	if(decimal == nil || decimal[0] == '\0')
		decimal = ".";
	if(thousands == nil)
		thousands = ",";
	if(grouping == nil)
		grouping = "\3";
	f->decimal = decimal;
	f->thousands = thousands;
	f->grouping = grouping;
}

/*
 * We are about to emit a digit in e.g. %'d.  If that digit would
 * overflow a thousands (e.g.) grouping, tell the caller to emit
 * the thousands separator.  Always advance the digit counter
 * and pointer into the grouping descriptor.
 */
int
__needsep(int *ndig, char **grouping)
{
	int group;
	
	(*ndig)++;
	group = *(unsigned char*)*grouping;
	/* CHAR_MAX means no further grouping. \0 means we got the empty string */
	if(group == 0xFF || group == 0x7f || group == 0x00)
		return 0;
	if(*ndig > group){
		/* if we're at end of string, continue with this grouping; else advance */
		if((*grouping)[1] != '\0')
			(*grouping)++;
		*ndig = 1;
		return 1;
	}
	return 0;
}

/* -------------- fmtlock.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

void
__fmtlock(void)
{
}

void
__fmtunlock(void)
{
}
/* -------------- fmtnull.c --------------- */
/* Copyright (c) 2004 Google Inc.; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

/*
 * Absorb output without using resources.
 */
static Rune nullbuf[32];

static int
__fmtnullflush(Fmt *f)
{
	f->to = nullbuf;
	f->nfmt = 0;
	return 0;
}

int
fmtnullinit(Fmt *f)
{
	memset(&f, 0, sizeof *f);
	f->runes = 1;
	f->start = nullbuf;
	f->to = nullbuf;
	f->stop = nullbuf+nelem(nullbuf);
	f->flush = __fmtnullflush;
	fmtlocaleinit(f, nil, nil, nil);
	return 0;
}

/* -------------- fmtprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

/*
 * format a string into the output buffer
 * designed for formats which themselves call fmt,
 * but ignore any width flags
 */
int
fmtprint(Fmt *f, char *fmt, ...)
{
	va_list va;
	int n;

	f->flags = 0;
	f->width = 0;
	f->prec = 0;
	VA_COPY(va, f->args);
	VA_END(f->args);
	va_start(f->args, fmt);
	n = dofmt(f, fmt);
	va_end(f->args);
	f->flags = 0;
	f->width = 0;
	f->prec = 0;
	VA_COPY(f->args,va);
	VA_END(va);
	if(n >= 0)
		return 0;
	return n;
}

/* -------------- fmtquote.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

/*
 * How many bytes of output UTF will be produced by quoting (if necessary) this string?
 * How many runes? How much of the input will be consumed?
 * The parameter q is filled in by __quotesetup.
 * The string may be UTF or Runes (s or r).
 * Return count does not include NUL.
 * Terminate the scan at the first of:
 *	NUL in input
 *	count exceeded in input
 *	count exceeded on output
 * *ninp is set to number of input bytes accepted.
 * nin may be <0 initially, to avoid checking input by count.
 */
void
__quotesetup(char *s, Rune *r, int nin, int nout, Quoteinfo *q, int sharp, int runesout)
{
	int w;
	Rune c;

	q->quoted = 0;
	q->nbytesout = 0;
	q->nrunesout = 0;
	q->nbytesin = 0;
	q->nrunesin = 0;
	if(sharp || nin==0 || (s && *s=='\0') || (r && *r=='\0')){
		if(nout < 2)
			return;
		q->quoted = 1;
		q->nbytesout = 2;
		q->nrunesout = 2;
	}
	for(; nin!=0; nin--){
		if(s)
			w = chartorune(&c, s);
		else{
			c = *r;
			w = runelen(c);
		}

		if(c == '\0')
			break;
		if(runesout){
			if(q->nrunesout+1 > nout)
				break;
		}else{
			if(q->nbytesout+w > nout)
				break;
		}

		if((c <= L' ') || (c == L'\'') || (fmtdoquote!=nil && fmtdoquote(c))){
			if(!q->quoted){
				if(runesout){
					if(1+q->nrunesout+1+1 > nout)	/* no room for quotes */
						break;
				}else{
					if(1+q->nbytesout+w+1 > nout)	/* no room for quotes */
						break;
				}
				q->nrunesout += 2;	/* include quotes */
				q->nbytesout += 2;	/* include quotes */
				q->quoted = 1;
			}
			if(c == '\'')	{
				if(runesout){
					if(1+q->nrunesout+1 > nout)	/* no room for quotes */
						break;
				}else{
					if(1+q->nbytesout+w > nout)	/* no room for quotes */
						break;
				}
				q->nbytesout++;
				q->nrunesout++;	/* quotes reproduce as two characters */
			}
		}

		/* advance input */
		if(s)
			s += w;
		else
			r++;
		q->nbytesin += w;
		q->nrunesin++;

		/* advance output */
		q->nbytesout += w;
		q->nrunesout++;

#ifndef PLAN9PORT
		/* ANSI requires precision in bytes, not Runes. */
		nin-= w-1;	/* and then n-- in the loop */
#endif
	}
}

static int
qstrfmt(char *sin, Rune *rin, Quoteinfo *q, Fmt *f)
{
	Rune r, *rm, *rme;
	char *t, *s, *m, *me;
	Rune *rt, *rs;
	ulong fl;
	int nc, w;

	m = sin;
	me = m + q->nbytesin;
	rm = rin;
	rme = rm + q->nrunesin;

	fl = f->flags;
	w = 0;
	if(fl & FmtWidth)
		w = f->width;
	if(f->runes){
		if(!(fl & FmtLeft) && __rfmtpad(f, w - q->nrunesout) < 0)
			return -1;
	}else{
		if(!(fl & FmtLeft) && __fmtpad(f, w - q->nbytesout) < 0)
			return -1;
	}
	t = (char*)f->to;
	s = (char*)f->stop;
	rt = (Rune*)f->to;
	rs = (Rune*)f->stop;
	if(f->runes)
		FMTRCHAR(f, rt, rs, '\'');
	else
		FMTRUNE(f, t, s, '\'');
	for(nc = q->nrunesin; nc > 0; nc--){
		if(sin){
			r = *(uchar*)m;
			if(r < Runeself)
				m++;
			else if((me - m) >= UTFmax || fullrune(m, me-m))
				m += chartorune(&r, m);
			else
				break;
		}else{
			if(rm >= rme)
				break;
			r = *(uchar*)rm++;
		}
		if(f->runes){
			FMTRCHAR(f, rt, rs, r);
			if(r == '\'')
				FMTRCHAR(f, rt, rs, r);
		}else{
			FMTRUNE(f, t, s, r);
			if(r == '\'')
				FMTRUNE(f, t, s, r);
		}
	}

	if(f->runes){
		FMTRCHAR(f, rt, rs, '\'');
		USED(rs);
		f->nfmt += rt - (Rune *)f->to;
		f->to = rt;
		if(fl & FmtLeft && __rfmtpad(f, w - q->nrunesout) < 0)
			return -1;
	}else{
		FMTRUNE(f, t, s, '\'');
		USED(s);
		f->nfmt += t - (char *)f->to;
		f->to = t;
		if(fl & FmtLeft && __fmtpad(f, w - q->nbytesout) < 0)
			return -1;
	}
	return 0;
}

int
__quotestrfmt(int runesin, Fmt *f)
{
	int nin, outlen;
	Rune *r;
	char *s;
	Quoteinfo q;

	nin = -1;
	if(f->flags&FmtPrec)
		nin = f->prec;
	if(runesin){
		r = va_arg(f->args, Rune *);
		s = nil;
	}else{
		s = va_arg(f->args, char *);
		r = nil;
	}
	if(!s && !r)
		return __fmtcpy(f, (void*)"<nil>", 5, 5);

	if(f->flush)
		outlen = 0x7FFFFFFF;	/* if we can flush, no output limit */
	else if(f->runes)
		outlen = (Rune*)f->stop - (Rune*)f->to;
	else
		outlen = (char*)f->stop - (char*)f->to;

	__quotesetup(s, r, nin, outlen, &q, f->flags&FmtSharp, f->runes);
/*print("bytes in %d bytes out %d runes in %d runesout %d\n", q.nbytesin, q.nbytesout, q.nrunesin, q.nrunesout); */

	if(runesin){
		if(!q.quoted)
			return __fmtrcpy(f, r, q.nrunesin);
		return qstrfmt(nil, r, &q, f);
	}

	if(!q.quoted)
		return __fmtcpy(f, s, q.nrunesin, q.nbytesin);
	return qstrfmt(s, nil, &q, f);
}

int
quotestrfmt(Fmt *f)
{
	return __quotestrfmt(0, f);
}

int
quoterunestrfmt(Fmt *f)
{
	return __quotestrfmt(1, f);
}

void
quotefmtinstall(void)
{
	fmtinstall('q', quotestrfmt);
	fmtinstall('Q', quoterunestrfmt);
}

int
__needsquotes(char *s, int *quotelenp)
{
	Quoteinfo q;

	__quotesetup(s, nil, -1, 0x7FFFFFFF, &q, 0, 0);
	*quotelenp = q.nbytesout;

	return q.quoted;
}

int
__runeneedsquotes(Rune *r, int *quotelenp)
{
	Quoteinfo q;

	__quotesetup(nil, r, -1, 0x7FFFFFFF, &q, 0, 0);
	*quotelenp = q.nrunesout;

	return q.quoted;
}
/* -------------- fmtrune.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

int
fmtrune(Fmt *f, int r)
{
	Rune *rt;
	char *t;
	int n;

	if(f->runes){
		rt = (Rune*)f->to;
		FMTRCHAR(f, rt, f->stop, r);
		f->to = rt;
		n = 1;
	}else{
		t = (char*)f->to;
		FMTRUNE(f, t, f->stop, r);
		n = t - (char*)f->to;
		f->to = t;
	}
	f->nfmt += n;
	return 0;
}
/* -------------- fmtstr.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdlib.h>
// #include <stdarg.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

char*
fmtstrflush(Fmt *f)
{
	if(f->start == nil)
		return nil;
	*(char*)f->to = '\0';
	f->to = f->start;
	return (char*)f->start;
}
/* -------------- fmtvprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"


/*
 * format a string into the output buffer
 * designed for formats which themselves call fmt,
 * but ignore any width flags
 */
int
fmtvprint(Fmt *f, char *fmt, va_list args)
{
	va_list va;
	int n;

	f->flags = 0;
	f->width = 0;
	f->prec = 0;
	VA_COPY(va,f->args);
	VA_END(f->args);
	VA_COPY(f->args,args);
	n = dofmt(f, fmt);
	f->flags = 0;
	f->width = 0;
	f->prec = 0;
	VA_END(f->args);
	VA_COPY(f->args,va);
	VA_END(va);
	if(n >= 0)
		return 0;
	return n;
}

/* -------------- fprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

int
fprint(int fd, char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = vfprint(fd, fmt, args);
	va_end(args);
	return n;
}
/* -------------- nan64.c --------------- */
/*
 * 64-bit IEEE not-a-number routines.
 * This is big/little-endian portable assuming that 
 * the 64-bit doubles and 64-bit integers have the
 * same byte ordering.
 */

// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

#if defined (__APPLE__) || (__powerpc__)
#define _NEEDLL
#endif

static uvlong uvnan    = ((uvlong)0x7FF00000<<32)|0x00000001;
static uvlong uvinf    = ((uvlong)0x7FF00000<<32)|0x00000000;
static uvlong uvneginf = ((uvlong)0xFFF00000<<32)|0x00000000;

double
__NaN(void)
{
	uvlong *p;

	/* gcc complains about "return *(double*)&uvnan;" */
	p = &uvnan;
	return *(double*)p;
}

int
__isNaN(double d)
{
	uvlong x;
	double *p;

	p = &d;
	x = *(uvlong*)p;
	return (ulong)(x>>32)==0x7FF00000 && !__isInf(d, 0);
}

double
__Inf(int sign)
{
	uvlong *p;

	if(sign < 0)
		p = &uvinf;
	else
		p = &uvneginf;
	return *(double*)p;
}

int
__isInf(double d, int sign)
{
	uvlong x;
	double *p;

	p = &d;
	x = *(uvlong*)p;
	if(sign == 0)
		return x==uvinf || x==uvneginf;
	else if(sign > 0)
		return x==uvinf;
	else
		return x==uvneginf;
}
/* -------------- pow10.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

/*
 * this table might overflow 127-bit exponent representations.
 * in that case, truncate it after 1.0e38.
 * it is important to get all one can from this
 * routine since it is used in atof to scale numbers.
 * the presumption is that C converts fp numbers better
 * than multipication of lower powers of 10.
 */

static
double	tab[] =
{
	1.0e0, 1.0e1, 1.0e2, 1.0e3, 1.0e4, 1.0e5, 1.0e6, 1.0e7, 1.0e8, 1.0e9,
	1.0e10,1.0e11,1.0e12,1.0e13,1.0e14,1.0e15,1.0e16,1.0e17,1.0e18,1.0e19,
	1.0e20,1.0e21,1.0e22,1.0e23,1.0e24,1.0e25,1.0e26,1.0e27,1.0e28,1.0e29,
	1.0e30,1.0e31,1.0e32,1.0e33,1.0e34,1.0e35,1.0e36,1.0e37,1.0e38,1.0e39,
	1.0e40,1.0e41,1.0e42,1.0e43,1.0e44,1.0e45,1.0e46,1.0e47,1.0e48,1.0e49,
	1.0e50,1.0e51,1.0e52,1.0e53,1.0e54,1.0e55,1.0e56,1.0e57,1.0e58,1.0e59,
	1.0e60,1.0e61,1.0e62,1.0e63,1.0e64,1.0e65,1.0e66,1.0e67,1.0e68,1.0e69,
};

double
__fmtpow10(int n)
{
	int m;

	if(n < 0) {
		n = -n;
		if(n < (int)(sizeof(tab)/sizeof(tab[0])))
			return 1/tab[n];
		m = n/2;
		return __fmtpow10(-m) * __fmtpow10(m-n);
	}
	if(n < (int)(sizeof(tab)/sizeof(tab[0])))
		return tab[n];
	m = n/2;
	return __fmtpow10(m) * __fmtpow10(n-m);
}
/* -------------- print.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

/*
int
print(char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = vfprint(1, fmt, args);
	va_end(args);
	return n;
}
*/

/* -------------- runefmtstr.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <stdlib.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

Rune*
runefmtstrflush(Fmt *f)
{
	if(f->start == nil)
		return nil;
	*(Rune*)f->to = '\0';
	f->to = f->start;
	return f->start;
}
/* -------------- runeseprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

Rune*
runeseprint(Rune *buf, Rune *e, char *fmt, ...)
{
	Rune *p;
	va_list args;

	va_start(args, fmt);
	p = runevseprint(buf, e, fmt, args);
	va_end(args);
	return p;
}
/* -------------- runesmprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

Rune*
runesmprint(char *fmt, ...)
{
	va_list args;
	Rune *p;

	va_start(args, fmt);
	p = runevsmprint(fmt, args);
	va_end(args);
	return p;
}
/* -------------- runesnprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

int
runesnprint(Rune *buf, int len, char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = runevsnprint(buf, len, fmt, args);
	va_end(args);
	return n;
}

/* -------------- runesprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

int
runesprint(Rune *buf, char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = runevsnprint(buf, 256, fmt, args);
	va_end(args);
	return n;
}
/* -------------- runevseprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

Rune*
runevseprint(Rune *buf, Rune *e, char *fmt, va_list args)
{
	Fmt f;

	if(e <= buf)
		return nil;
	f.runes = 1;
	f.start = buf;
	f.to = buf;
	f.stop = e - 1;
	f.flush = nil;
	f.farg = nil;
	f.nfmt = 0;
	VA_COPY(f.args,args);
	fmtlocaleinit(&f, nil, nil, nil);
	dofmt(&f, fmt);
	VA_END(f.args);
	*(Rune*)f.to = '\0';
	return (Rune*)f.to;
}

/* -------------- runevsmprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
/*
 * Plan 9 port version must include libc.h in order to 
 * get Plan 9 debugging malloc, which sometimes returns
 * different pointers than the standard malloc. 
 */
#ifdef PLAN9PORT
// #include <u.h>
// #include <libc.h>
// #include "fmtdef.h"
#else
// #include <stdlib.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"
#endif

static int
runeFmtStrFlush(Fmt *f)
{
	Rune *s;
	int n;

	if(f->start == nil)
		return 0;
	n = (uintptr)f->farg;
	n *= 2;
	s = (Rune*)f->start;
	f->start = realloc(s, sizeof(Rune)*n);
	if(f->start == nil){
		f->farg = nil;
		f->to = nil;
		f->stop = nil;
		free(s);
		return 0;
	}
	f->farg = (void*)(uintptr)n;
	f->to = (Rune*)f->start + ((Rune*)f->to - s);
	f->stop = (Rune*)f->start + n - 1;
	return 1;
}

int
runefmtstrinit(Fmt *f)
{
	int n;

	memset(f, 0, sizeof *f);
	f->runes = 1;
	n = 32;
	f->start = malloc(sizeof(Rune)*n);
	if(f->start == nil)
		return -1;
	f->to = f->start;
	f->stop = (Rune*)f->start + n - 1;
	f->flush = runeFmtStrFlush;
	f->farg = (void*)(uintptr)n;
	f->nfmt = 0;
	fmtlocaleinit(f, nil, nil, nil);
	return 0;
}

/*
 * print into an allocated string buffer
 */
Rune*
runevsmprint(char *fmt, va_list args)
{
	Fmt f;
	int n;

	if(runefmtstrinit(&f) < 0)
		return nil;
	VA_COPY(f.args,args);
	n = dofmt(&f, fmt);
	VA_END(f.args);
	if(f.start == nil)
		return nil;
	if(n < 0){
		free(f.start);
		return nil;
	}
	*(Rune*)f.to = '\0';
	return (Rune*)f.start;
}
/* -------------- runevsnprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

int
runevsnprint(Rune *buf, int len, char *fmt, va_list args)
{
	Fmt f;

	if(len <= 0)
		return -1;
	f.runes = 1;
	f.start = buf;
	f.to = buf;
	f.stop = buf + len - 1;
	f.flush = nil;
	f.farg = nil;
	f.nfmt = 0;
	VA_COPY(f.args,args);
	fmtlocaleinit(&f, nil, nil, nil);
	dofmt(&f, fmt);
	VA_END(f.args);
	*(Rune*)f.to = '\0';
	return (Rune*)f.to - buf;
}
/* -------------- seprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

char*
seprint(char *buf, char *e, char *fmt, ...)
{
	char *p;
	va_list args;

	va_start(args, fmt);
	p = vseprint(buf, e, fmt, args);
	va_end(args);
	return p;
}
/* -------------- smprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

char*
smprint(char *fmt, ...)
{
	va_list args;
	char *p;

	va_start(args, fmt);
	p = vsmprint(fmt, args);
	va_end(args);
	return p;
}
/* -------------- snprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

int
snprint(char *buf, int len, char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = vsnprint(buf, len, fmt, args);
	va_end(args);
	return n;
}

/* -------------- sprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include <fmt.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

int
sprint(char *buf, char *fmt, ...)
{
	int n;
	uint len;
	va_list args;

	len = 1<<30;  /* big number, but sprint is deprecated anyway */
	/*
	 * on PowerPC, the stack is near the top of memory, so
	 * we must be sure not to overflow a 32-bit pointer.
	 */
	if(buf+len < buf)
		len = -(uintptr)buf-1;

	va_start(args, fmt);
	n = vsnprint(buf, len, fmt, args);
	va_end(args);
	return n;
}
/* -------------- strtod.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdlib.h>
// #include <math.h>
// #include <ctype.h>
// #include <stdlib.h>
// #include <string.h>
// #include <errno.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

static ulong
umuldiv(ulong a, ulong b, ulong c)
{
	double d;

	d = ((double)a * (double)b) / (double)c;
	if(d >= 4294967295.)
		d = 4294967295.;
	return (ulong)d;
}

/*
 * This routine will convert to arbitrary precision
 * floating point entirely in multi-precision fixed.
 * The answer is the closest floating point number to
 * the given decimal number. Exactly half way are
 * rounded ala ieee rules.
 * Method is to scale input decimal between .500 and .999...
 * with external power of 2, then binary search for the
 * closest mantissa to this decimal number.
 * Nmant is is the required precision. (53 for ieee dp)
 * Nbits is the max number of bits/word. (must be <= 28)
 * Prec is calculated - the number of words of fixed mantissa.
 */
enum
{
	Nbits	= 28,				/* bits safely represented in a ulong */
	Nmant	= 53,				/* bits of precision required */
	Prec	= (Nmant+Nbits+1)/Nbits,	/* words of Nbits each to represent mantissa */
	Sigbit	= 1<<(Prec*Nbits-Nmant),	/* first significant bit of Prec-th word */
	Ndig	= 1500,
	One	= (ulong)(1<<Nbits),
	Half	= (ulong)(One>>1),
	Maxe	= 310,

	Fsign	= 1<<0,		/* found - */
	Fesign	= 1<<1,		/* found e- */
	Fdpoint	= 1<<2,		/* found . */

	S0	= 0,		/* _		_S0	+S1	#S2	.S3 */
	S1,			/* _+		#S2	.S3 */
	S2,			/* _+#		#S2	.S4	eS5 */
	S3,			/* _+.		#S4 */
	S4,			/* _+#.#	#S4	eS5 */
	S5,			/* _+#.#e	+S6	#S7 */
	S6,			/* _+#.#e+	#S7 */
	S7			/* _+#.#e+#	#S7 */
};

static	int	xcmp(char*, char*);
static	int	fpcmp(char*, ulong*);
static	void	frnorm(ulong*);
static	void	divascii(char*, int*, int*, int*);
static	void	mulascii(char*, int*, int*, int*);

typedef	struct	Tab	Tab;
struct	Tab
{
	int	bp;
	int	siz;
	char*	cmp;
};

double
fmtstrtod(const char *as, char **aas)
{
	int na, ex, dp, bp, c, i, flag, state;
	ulong low[Prec], hig[Prec], mid[Prec];
	double d;
	char *s, a[Ndig];

	flag = 0;	/* Fsign, Fesign, Fdpoint */
	na = 0;		/* number of digits of a[] */
	dp = 0;		/* na of decimal point */
	ex = 0;		/* exonent */

	state = S0;
	for(s=(char*)as;; s++) {
		c = *s;
		if(c >= '0' && c <= '9') {
			switch(state) {
			case S0:
			case S1:
			case S2:
				state = S2;
				break;
			case S3:
			case S4:
				state = S4;
				break;

			case S5:
			case S6:
			case S7:
				state = S7;
				ex = ex*10 + (c-'0');
				continue;
			}
			if(na == 0 && c == '0') {
				dp--;
				continue;
			}
			if(na < Ndig-50)
				a[na++] = c;
			continue;
		}
		switch(c) {
		case '\t':
		case '\n':
		case '\v':
		case '\f':
		case '\r':
		case ' ':
			if(state == S0)
				continue;
			break;
		case '-':
			if(state == S0)
				flag |= Fsign;
			else
				flag |= Fesign;
		case '+':
			if(state == S0)
				state = S1;
			else
			if(state == S5)
				state = S6;
			else
				break;	/* syntax */
			continue;
		case '.':
			flag |= Fdpoint;
			dp = na;
			if(state == S0 || state == S1) {
				state = S3;
				continue;
			}
			if(state == S2) {
				state = S4;
				continue;
			}
			break;
		case 'e':
		case 'E':
			if(state == S2 || state == S4) {
				state = S5;
				continue;
			}
			break;
		}
		break;
	}

	/*
	 * clean up return char-pointer
	 */
	switch(state) {
	case S0:
		if(xcmp(s, "nan") == 0) {
			if(aas != nil)
				*aas = s+3;
			goto retnan;
		}
	case S1:
		if(xcmp(s, "infinity") == 0) {
			if(aas != nil)
				*aas = s+8;
			goto retinf;
		}
		if(xcmp(s, "inf") == 0) {
			if(aas != nil)
				*aas = s+3;
			goto retinf;
		}
	case S3:
		if(aas != nil)
			*aas = (char*)as;
		goto ret0;	/* no digits found */
	case S6:
		s--;		/* back over +- */
	case S5:
		s--;		/* back over e */
		break;
	}
	if(aas != nil)
		*aas = s;

	if(flag & Fdpoint)
	while(na > 0 && a[na-1] == '0')
		na--;
	if(na == 0)
		goto ret0;	/* zero */
	a[na] = 0;
	if(!(flag & Fdpoint))
		dp = na;
	if(flag & Fesign)
		ex = -ex;
	dp += ex;
	if(dp < -Maxe){
		errno = ERANGE;
		goto ret0;	/* underflow by exp */
	} else
	if(dp > +Maxe)
		goto retinf;	/* overflow by exp */

	/*
	 * normalize the decimal ascii number
	 * to range .[5-9][0-9]* e0
	 */
	bp = 0;		/* binary exponent */
	while(dp > 0)
		divascii(a, &na, &dp, &bp);
	while(dp < 0 || a[0] < '5')
		mulascii(a, &na, &dp, &bp);

	/* close approx by naive conversion */
	mid[0] = 0;
	mid[1] = 1;
	for(i=0; (c=a[i]) != '\0'; i++) {
		mid[0] = mid[0]*10 + (c-'0');
		mid[1] = mid[1]*10;
		if(i >= 8)
			break;
	}
	low[0] = umuldiv(mid[0], One, mid[1]);
	hig[0] = umuldiv(mid[0]+1, One, mid[1]);
	for(i=1; i<Prec; i++) {
		low[i] = 0;
		hig[i] = One-1;
	}

	/* binary search for closest mantissa */
	for(;;) {
		/* mid = (hig + low) / 2 */
		c = 0;
		for(i=0; i<Prec; i++) {
			mid[i] = hig[i] + low[i];
			if(c)
				mid[i] += One;
			c = mid[i] & 1;
			mid[i] >>= 1;
		}
		frnorm(mid);

		/* compare */
		c = fpcmp(a, mid);
		if(c > 0) {
			c = 1;
			for(i=0; i<Prec; i++)
				if(low[i] != mid[i]) {
					c = 0;
					low[i] = mid[i];
				}
			if(c)
				break;	/* between mid and hig */
			continue;
		}
		if(c < 0) {
			for(i=0; i<Prec; i++)
				hig[i] = mid[i];
			continue;
		}

		/* only hard part is if even/odd roundings wants to go up */
		c = mid[Prec-1] & (Sigbit-1);
		if(c == Sigbit/2 && (mid[Prec-1]&Sigbit) == 0)
			mid[Prec-1] -= c;
		break;	/* exactly mid */
	}

	/* normal rounding applies */
	c = mid[Prec-1] & (Sigbit-1);
	mid[Prec-1] -= c;
	if(c >= Sigbit/2) {
		mid[Prec-1] += Sigbit;
		frnorm(mid);
	}
	goto out;

ret0:
	return 0;

retnan:
	return __NaN();

retinf:
	/*
	 * Unix strtod requires these.  Plan 9 would return Inf(0) or Inf(-1). */
	errno = ERANGE;
	if(flag & Fsign)
		return -HUGE_VAL;
	return HUGE_VAL;

out:
	d = 0;
	for(i=0; i<Prec; i++)
		d = d*One + mid[i];
	if(flag & Fsign)
		d = -d;
	d = ldexp(d, bp - Prec*Nbits);
	if(d == 0){	/* underflow */
		errno = ERANGE;
	}
	return d;
}

static void
frnorm(ulong *f)
{
	int i, c;

	c = 0;
	for(i=Prec-1; i>0; i--) {
		f[i] += c;
		c = f[i] >> Nbits;
		f[i] &= One-1;
	}
	f[0] += c;
}

static int
fpcmp(char *a, ulong* f)
{
	ulong tf[Prec];
	int i, d, c;

	for(i=0; i<Prec; i++)
		tf[i] = f[i];

	for(;;) {
		/* tf *= 10 */
		for(i=0; i<Prec; i++)
			tf[i] = tf[i]*10;
		frnorm(tf);
		d = (tf[0] >> Nbits) + '0';
		tf[0] &= One-1;

		/* compare next digit */
		c = *a;
		if(c == 0) {
			if('0' < d)
				return -1;
			if(tf[0] != 0)
				goto cont;
			for(i=1; i<Prec; i++)
				if(tf[i] != 0)
					goto cont;
			return 0;
		}
		if(c > d)
			return +1;
		if(c < d)
			return -1;
		a++;
	cont:;
	}
}

static void
divby(char *a, int *na, int b)
{
	int n, c;
	char *p;

	p = a;
	n = 0;
	while(n>>b == 0) {
		c = *a++;
		if(c == 0) {
			while(n) {
				c = n*10;
				if(c>>b)
					break;
				n = c;
			}
			goto xx;
		}
		n = n*10 + c-'0';
		(*na)--;
	}
	for(;;) {
		c = n>>b;
		n -= c<<b;
		*p++ = c + '0';
		c = *a++;
		if(c == 0)
			break;
		n = n*10 + c-'0';
	}
	(*na)++;
xx:
	while(n) {
		n = n*10;
		c = n>>b;
		n -= c<<b;
		*p++ = c + '0';
		(*na)++;
	}
	*p = 0;
}

static	Tab	tab1[] =
{
	{  1,  0, "" },
	{  3,  1, "7" },
	{  6,  2, "63" },
	{  9,  3, "511" },
	{ 13,  4, "8191" },
	{ 16,  5, "65535" },
	{ 19,  6, "524287" },
	{ 23,  7, "8388607" },
	{ 26,  8, "67108863" },
	{ 27,  9, "134217727" },
};

static void
divascii(char *a, int *na, int *dp, int *bp)
{
	int b, d;
	Tab *t;

	d = *dp;
	if(d >= (int)(nelem(tab1)))
		d = (int)(nelem(tab1))-1;
	t = tab1 + d;
	b = t->bp;
	if(memcmp(a, t->cmp, t->siz) > 0)
		d--;
	*dp -= d;
	*bp += b;
	divby(a, na, b);
}

static void
mulby(char *a, char *p, char *q, int b)
{
	int n, c;

	n = 0;
	*p = 0;
	for(;;) {
		q--;
		if(q < a)
			break;
		c = *q - '0';
		c = (c<<b) + n;
		n = c/10;
		c -= n*10;
		p--;
		*p = c + '0';
	}
	while(n) {
		c = n;
		n = c/10;
		c -= n*10;
		p--;
		*p = c + '0';
	}
}

static	Tab	tab2[] =
{
	{  1,  1, "" },				/* dp = 0-0 */
	{  3,  3, "125" },
	{  6,  5, "15625" },
	{  9,  7, "1953125" },
	{ 13, 10, "1220703125" },
	{ 16, 12, "152587890625" },
	{ 19, 14, "19073486328125" },
	{ 23, 17, "11920928955078125" },
	{ 26, 19, "1490116119384765625" },
	{ 27, 19, "7450580596923828125" },		/* dp 8-9 */
};

static void
mulascii(char *a, int *na, int *dp, int *bp)
{
	char *p;
	int d, b;
	Tab *t;

	d = -*dp;
	if(d >= (int)(nelem(tab2)))
		d = (int)(nelem(tab2))-1;
	t = tab2 + d;
	b = t->bp;
	if(memcmp(a, t->cmp, t->siz) < 0)
		d--;
	p = a + *na;
	*bp -= b;
	*dp += d;
	*na += d;
	mulby(a, p+d, p, b);
}

static int
xcmp(char *a, char *b)
{
	int c1, c2;

	while((c1 = *b++) != '\0') {
		c2 = *a++;
		if(isupper(c2))
			c2 = tolower(c2);
		if(c1 != c2)
			return 1;
	}
	return 0;
}
/* -------------- vfprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

int
vfprint(int fd, char *fmt, va_list args)
{
	Fmt f;
	char buf[256];
	int n;

	fmtfdinit(&f, fd, buf, sizeof(buf));
	VA_COPY(f.args,args);
	n = dofmt(&f, fmt);
	VA_END(f.args);
	if(n > 0 && __fmtFdFlush(&f) == 0)
		return -1;
	return n;
}
/* -------------- vseprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdarg.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

char*
vseprint(char *buf, char *e, char *fmt, va_list args)
{
	Fmt f;

	if(e <= buf)
		return nil;
	f.runes = 0;
	f.start = buf;
	f.to = buf;
	f.stop = e - 1;
	f.flush = 0;
	f.farg = nil;
	f.nfmt = 0;
	VA_COPY(f.args,args);
	fmtlocaleinit(&f, nil, nil, nil);
	dofmt(&f, fmt);
	VA_END(f.args);
	*(char*)f.to = '\0';
	return (char*)f.to;
}

/* -------------- vsmprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
/*
 * Plan 9 port version must include libc.h in order to 
 * get Plan 9 debugging malloc, which sometimes returns
 * different pointers than the standard malloc. 
 */
#ifdef PLAN9PORT
// #include <u.h>
// #include <libc.h>
// #include "fmtdef.h"
#else
// #include <stdlib.h>
// #include <string.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"
#endif

static int
fmtStrFlush(Fmt *f)
{
	char *s;
	int n;

	if(f->start == nil)
		return 0;
	n = (uintptr)f->farg;
	n *= 2;
	s = (char*)f->start;
	f->start = realloc(s, n);
	if(f->start == nil){
		f->farg = nil;
		f->to = nil;
		f->stop = nil;
		free(s);
		return 0;
	}
	f->farg = (void*)(uintptr)n;
	f->to = (char*)f->start + ((char*)f->to - s);
	f->stop = (char*)f->start + n - 1;
	return 1;
}

int
fmtstrinit(Fmt *f)
{
	int n;

	memset(f, 0, sizeof *f);
	f->runes = 0;
	n = 32;
	f->start = malloc(n);
	if(f->start == nil)
		return -1;
	f->to = f->start;
	f->stop = (char*)f->start + n - 1;
	f->flush = fmtStrFlush;
	f->farg = (void*)(uintptr)n;
	f->nfmt = 0;
	fmtlocaleinit(f, nil, nil, nil);
	return 0;
}

/*
 * print into an allocated string buffer
 */
char*
vsmprint(char *fmt, va_list args)
{
	Fmt f;
	int n;

	if(fmtstrinit(&f) < 0)
		return nil;
	VA_COPY(f.args,args);
	n = dofmt(&f, fmt);
	VA_END(f.args);
	if(n < 0){
		free(f.start);
		return nil;
	}
	return fmtstrflush(&f);
}
/* -------------- vsnprint.c --------------- */
/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
// #include <stdlib.h>
// #include <stdarg.h>
// #include "plan9.h"
// #include "fmt.h"
// #include "fmtdef.h"

int
vsnprint(char *buf, int len, char *fmt, va_list args)
{
	Fmt f;

	if(len <= 0)
		return -1;
	f.runes = 0;
	f.start = buf;
	f.to = buf;
	f.stop = buf + len - 1;
	f.flush = 0;
	f.farg = nil;
	f.nfmt = 0;
	VA_COPY(f.args,args);
	fmtlocaleinit(&f, nil, nil, nil);
	dofmt(&f, fmt);
	VA_END(f.args);
	*(char*)f.to = '\0';
	return (char*)f.to - buf;
}

int
__errfmt(Fmt *f)
{
	char *s;

	s = strerror(errno);
	return fmtstrcpy(f, s);
}

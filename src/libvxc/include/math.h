/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#ifndef _MATH_H_
#define	_MATH_H_


/*
 * ANSI/POSIX
 */
extern const union __infinity_un {
	unsigned char	__uc[8];
	double		__ud;
} __infinity;

extern const union __nan_un {
	unsigned char	__uc[sizeof(float)];
	float		__uf;
} __nan;

#define	HUGE_VAL	__builtin_huge_val()

#define	FP_ILOGB0	(-INT_MAX)
#define	FP_ILOGBNAN	INT_MAX

#define	HUGE_VALF	__builtin_huge_valf()
#define	HUGE_VALL	__builtin_huge_vall()
#define	INFINITY	__builtin_inf()
#define	NAN		__builtin_nan("")

#define	MATH_ERRNO	1
#define	MATH_ERREXCEPT	2
#define	math_errhandling	MATH_ERREXCEPT

#define	FP_FAST_FMAF

/* Symbolic constants to classify floating point numbers. */
#define	FP_INFINITE	0x01
#define	FP_NAN		0x02
#define	FP_NORMAL	0x04
#define	FP_SUBNORMAL	0x08
#define	FP_ZERO		0x10
#define	fpclassify(x) \
    ((sizeof (x) == sizeof (float)) ? __fpclassifyf(x) \
    : (sizeof (x) == sizeof (double)) ? __fpclassifyd(x) \
    : __fpclassifyl(x))

#define	isfinite(x)					\
    ((sizeof (x) == sizeof (float)) ? __isfinitef(x)	\
    : (sizeof (x) == sizeof (double)) ? __isfinite(x)	\
    : __isfinitel(x))
#define	isinf(x)					\
    ((sizeof (x) == sizeof (float)) ? __isinff(x)	\
    : (sizeof (x) == sizeof (double)) ? isinf(x)	\
    : __isinfl(x))
#define	isnan(x)					\
    ((sizeof (x) == sizeof (float)) ? isnanf(x)		\
    : (sizeof (x) == sizeof (double)) ? isnan(x)	\
    : __isnanl(x))
#define	isnormal(x)					\
    ((sizeof (x) == sizeof (float)) ? __isnormalf(x)	\
    : (sizeof (x) == sizeof (double)) ? __isnormal(x)	\
    : __isnormall(x))

#define	isgreater(x, y)		__builtin_isgreater((x), (y))
#define	isgreaterequal(x, y)	__builtin_isgreaterequal((x), (y))
#define	isless(x, y)		__builtin_isless((x), (y))
#define	islessequal(x, y)	__builtin_islessequal((x), (y))
#define	islessgreater(x, y)	__builtin_islessgreater((x), (y))
#define	isunordered(x, y)	__builtin_isunordered((x), (y))

#define	signbit(x)					\
    ((sizeof (x) == sizeof (float)) ? __signbitf(x)	\
    : (sizeof (x) == sizeof (double)) ? __signbit(x)	\
    : __signbitl(x))


/* FLT_EVAL_METHOD = 0 (float.h) */
typedef	double			double_t;
typedef	float			float_t;


/*
 * XOPEN/SVID
 */
#define	M_E		2.7182818284590452354	/* e */
#define	M_LOG2E		1.4426950408889634074	/* log 2e */
#define	M_LOG10E	0.43429448190325182765	/* log 10e */
#define	M_LN2		0.69314718055994530942	/* log e2 */
#define	M_LN10		2.30258509299404568402	/* log e10 */
#define	M_PI		3.14159265358979323846	/* pi */
#define	M_PI_2		1.57079632679489661923	/* pi/2 */
#define	M_PI_4		0.78539816339744830962	/* pi/4 */
#define	M_1_PI		0.31830988618379067154	/* 1/pi */
#define	M_2_PI		0.63661977236758134308	/* 2/pi */
#define	M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#define	M_SQRT2		1.41421356237309504880	/* sqrt(2) */
#define	M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */

#define	MAXFLOAT	((float)3.40282346638528860e+38)
extern int signgam;

#if __BSD_VISIBLE
enum fdversion {fdlibm_ieee = -1, fdlibm_svid, fdlibm_xopen, fdlibm_posix};

#define _LIB_VERSION_TYPE enum fdversion
#define _LIB_VERSION _fdlib_version

/* if global variable _LIB_VERSION is not desirable, one may
 * change the following to be a constant by:
 *	#define _LIB_VERSION_TYPE const enum version
 * In that case, after one initializes the value _LIB_VERSION (see
 * s_lib_version.c) during compile time, it cannot be modified
 * in the middle of a program
 */
extern  _LIB_VERSION_TYPE  _LIB_VERSION;

#define _IEEE_  fdlibm_ieee
#define _SVID_  fdlibm_svid
#define _XOPEN_ fdlibm_xopen
#define _POSIX_ fdlibm_posix

/* We have a problem when using C++ since `exception' is a reserved
   name in C++.  */
#ifndef __cplusplus
struct exception {
	int type;
	char *name;
	double arg1;
	double arg2;
	double retval;
};
#endif

#if 0
/* Old value from 4.4BSD-Lite math.h; this is probably better. */
#define	HUGE		HUGE_VAL
#else
#define	HUGE		MAXFLOAT
#endif

#define X_TLOSS		1.41484755040568800000e+16	/* pi*2**52 */

#define	DOMAIN		1
#define	SING		2
#define	OVERFLOW	3
#define	UNDERFLOW	4
#define	TLOSS		5
#define	PLOSS		6

#endif /* __BSD_VISIBLE */

/*
 * Most of these functions depend on the rounding mode and have the side
 * effect of raising floating-point exceptions, so they are not declared
 * as __pure2.  In C99, FENV_ACCESS affects the purity of these functions.
 */
#define __pure2		__attribute__((__const__))

/*
 * ANSI/POSIX
 */
int	__fpclassifyd(double) __pure2;
int	__fpclassifyf(float) __pure2;
int	__fpclassifyl(long double) __pure2;
int	__isfinitef(float) __pure2;
int	__isfinite(double) __pure2;
int	__isfinitel(long double) __pure2;
int	__isinff(float) __pure2;
int	__isinfl(long double) __pure2;
int	__isnanl(long double) __pure2;
int	__isnormalf(float) __pure2;
int	__isnormal(double) __pure2;
int	__isnormall(long double) __pure2;
int	__signbit(double) __pure2;
int	__signbitf(float) __pure2;
int	__signbitl(long double) __pure2;

double	acos(double);
double	asin(double);
double	atan(double);
double	atan2(double, double);
double	cos(double);
double	sin(double);
double	tan(double);

double	cosh(double);
double	sinh(double);
double	tanh(double);

double	exp(double);
double	frexp(double, int *);	/* fundamentally !__pure2 */
double	ldexp(double, int);
double	log(double);
double	log10(double);
double	modf(double, double *);	/* fundamentally !__pure2 */

double	pow(double, double);
double	sqrt(double);

double	ceil(double);
double	fabs(double) __pure2;
double	floor(double);
double	fmod(double, double);

/*
 * These functions are not in C90.
 */
double	acosh(double);
double	asinh(double);
double	atanh(double);
double	cbrt(double);
double	erf(double);
double	erfc(double);
double	expm1(double);
double	hypot(double, double);
/* Our ilogb raises no exceptions; we side with IEEE-754R and C99, not POSIX */
int	ilogb(double) __pure2;
int	(isinf)(double) __pure2;
int	(isnan)(double) __pure2;
double	lgamma(double);
long long llrint(double);
long long llround(double);
double	log1p(double);
double	logb(double);
long	lrint(double);
long	lround(double);
double	nextafter(double, double);
double	remainder(double, double);
double	rint(double);

double	j0(double);
double	j1(double);
double	jn(int, double);
double	scalb(double, double);
double	y0(double);
double	y1(double);
double	yn(int, double);

double	copysign(double, double) __pure2;
double	fdim(double, double);
double	fmax(double, double) __pure2;
double	fmin(double, double) __pure2;
double	nearbyint(double);
double	round(double);
double	scalbln(double, long);
double	scalbn(double, int);
double	tgamma(double);
double	trunc(double);

/*
 * BSD math library entry points
 */
double	drem(double, double);
int	finite(double) __pure2;
int	isnanf(float) __pure2;

/*
 * Reentrant version of gamma & lgamma; passes signgam back by reference
 * as the second argument; user must allocate space for signgam.
 */
double	gamma_r(double, int *);
double	lgamma_r(double, int *);

/*
 * IEEE Test Vector
 */
double	significand(double);


/* float versions of ANSI/POSIX functions */
float	acosf(float);
float	asinf(float);
float	atanf(float);
float	atan2f(float, float);
float	cosf(float);
float	sinf(float);
float	tanf(float);

float	coshf(float);
float	sinhf(float);
float	tanhf(float);

float	expf(float);
float	expm1f(float);
float	frexpf(float, int *);	/* fundamentally !__pure2 */
int	ilogbf(float) __pure2;
float	ldexpf(float, int);
float	log10f(float);
float	log1pf(float);
float	logf(float);
float	modff(float, float *);	/* fundamentally !__pure2 */

float	powf(float, float);
float	sqrtf(float);

float	ceilf(float);
float	fabsf(float) __pure2;
float	floorf(float);
float	fmodf(float, float);
float	roundf(float);

float	erff(float);
float	erfcf(float);
float	hypotf(float, float);
float	lgammaf(float);

float	acoshf(float);
float	asinhf(float);
float	atanhf(float);
float	cbrtf(float);
float	logbf(float);
float	copysignf(float, float) __pure2;
long long llrintf(float);
long long llroundf(float);
long	lrintf(float);
long	lroundf(float);
float	nearbyintf(float);
float	nextafterf(float, float);
float	remainderf(float, float);
float	rintf(float);
float	scalblnf(float, long);
float	scalbnf(float, int);
float	truncf(float);

float	fdimf(float, float);
float	fmaxf(float, float) __pure2;
float	fminf(float, float) __pure2;

/*
 * float versions of BSD math library entry points
 */
float	dremf(float, float);
int	finitef(float) __pure2;
float	gammaf(float);
float	j0f(float);
float	j1f(float);
float	jnf(int, float);
float	scalbf(float, float);
float	y0f(float);
float	y1f(float);
float	ynf(int, float);

/*
 * Float versions of reentrant version of gamma & lgamma; passes
 * signgam back by reference as the second argument; user must
 * allocate space for signgam.
 */
float	gammaf_r(float, int *);
float	lgammaf_r(float, int *);

/*
 * float version of IEEE Test Vector
 */
float	significandf(float);

#endif /* !_MATH_H_ */

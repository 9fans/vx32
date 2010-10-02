#ifndef _STDINT_H
#define _STDINT_H


// Exact-width integer types
typedef char			int8_t;
typedef short			int16_t;
typedef int			int32_t;
typedef long long		int64_t;

typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int		uint32_t;
typedef unsigned long long	uint64_t;


// Fastest integer types of given width
typedef char			int_fast8_t;
typedef int			int_fast16_t;
typedef int			int_fast32_t;
typedef long long		int_fast64_t;

typedef unsigned char		uint_fast8_t;
typedef unsigned int		uint_fast16_t;
typedef unsigned int		uint_fast32_t;
typedef unsigned long long	uint_fast64_t;


// Pointer-size integer types
typedef int			intptr_t;
typedef unsigned long		uintptr_t;


// Maximum-size integer types
typedef long long		intmax_t;
typedef unsigned long long	uintmax_t;


#endif // _STDINT_H

// Stripped-down primitive printf-style formatting routines,
// used in common by printf, sprintf, fprintf, etc.
// This code is also used by both the kernel and user programs.
//
// Space or zero padding and a field width are supported for the numeric
// formats only. 


#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#include "ioprivate.h"


typedef unsigned char		uchar;
typedef unsigned long long	ull;

/*
 * Print a number (base <= 16) in reverse order,
 * using specified putch function and associated pointer putdat.
 */
static int
printnum(int (*putch)(int, void*), void *putdat,
	 ull num, unsigned base, int width, int padc)
{
	int nout;

	// first recursively print all preceding (more significant) digits
	if (num >= base) {
		nout = printnum(putch, putdat, num / base, base, width-1,
				padc);
		if (nout < 0)
			return -1;
	} else {
		// print any needed pad characters before first digit
		nout = 0;
		while (--width > 0) {
			if (putch(padc, putdat) < 0)
				return -1;
			nout++;
		}
	}

	// then print this (the least significant) digit
	if (putch("0123456789abcdef"[num % base], putdat) < 0)
		return -1;
	nout++;

	return nout;
}

// Main function to format and print a string.
int
vprintfmt(int (*putch)(int, void*), void *putdat, const char *fmt, va_list ap)
{
	register char *p;
	register int ch;
	int nout = 0;

	for (;;) {
		while ((ch = *(uchar *) fmt++) != '%') {
			if (ch == '\0')
				return nout;
			if (putch(ch, putdat) < 0)
				return -1;
			nout++;
		}

		// Process a %-escape sequence
		int padc = ' ';
		int width = 0;
		int length = 0;
		int base = 10;
		ull num;

	reswitch:
		switch (ch = *(uchar *) fmt++) {

		// flag to pad with 0's instead of spaces
		case '0':
			padc = '0';
			goto reswitch;

		// width field
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			for (width = 0;; ++fmt) {
				width = width * 10 + ch - '0';
				ch = *fmt;
				if (ch < '0' || ch > '9')
					break;
			}
			goto reswitch;

		case 'l':
			length++;
			goto reswitch;

		// character
		case 'c':
			if (putch(va_arg(ap, int), putdat) < 0)
				return -1;
			nout++;
			break;

		// string
		case 's':
			if ((p = va_arg(ap, char *)) == NULL)
					p = "(null)";
			while ((ch = *p++) != '\0') {
				if (putch(ch, putdat) < 0)
					return -1;
				nout++;
			}
			break;

		// signed decimal
		case 'd': ;
			// Pick up an appropriate-sized signed input value
			long long snum;
			if (length == 0)
				snum = va_arg(ap, int);
			else if (length == 1)
				snum = va_arg(ap, long);
			else
				snum = va_arg(ap, long long);

			// Output the minus sign if appropriate
			if (snum < 0) {
				if (putch('-', putdat) < 0)
					return -1;
				nout++;
				num = -snum;
			} else
				num = snum;
			goto number;

		// unsigned hexadecimal
		case 'x':
			base = 16;
			// fall thru...

		// unsigned decimal
		case 'u':
			// Pick up the appropriate-sized input argument
			if (length == 0)
				num = va_arg(ap, unsigned);
			else if (length == 1)
				num = va_arg(ap, unsigned long);
			else
				num = va_arg(ap, unsigned long long);

		number: ;
			// Print the number in the appropriate base
			int rc = printnum(putch, putdat, num, base, width,
						padc);
			if (rc < 0)
				return -1;
			nout += rc;
			break;

		// unrecognized escape sequence - just print it literally
		default:
			if (putch('%', putdat) < 0)
				return -1;
			nout++;
			/* FALLTHROUGH */

		// escaped '%' character
		case '%':
			if (putch(ch, putdat) < 0)
				return -1;
			nout++;
		}
	}
}

int
printfmt(int (*putch)(int, void*), void *putdat, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int rc = vprintfmt(putch, putdat, fmt, ap);
	va_end(ap);
	return rc;
}


#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdlib.h>

#include "ioprivate.h"

typedef unsigned char		uchar;

static void skipspace(FILE *fin)
{
	int c;
	
	while ((c = fgetc(fin)) != EOF) {
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			continue;
		ungetc(c, fin);
		break;
	}
}

// Main function to format and print a string.
int vfscanf(FILE *fin, const char *fmt, va_list ap)
{
	char *p;
	int ch, c;
	int nout = 0;
	int off0;
	int sign;

	off0 = ftell(fin);
	unsigned long long ull;

	for (;;) {
		while ((ch = *(uchar *) fmt++) != '%') {
			if (ch == '\0')
				return nout;
			if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
				skipspace(fin);
				continue;
			}
			if ((c = fgetc(fin)) != ch) {
				ungetc(c, fin);
				break;
			}
		}

		// Process a %-escape sequence
		int flag[256];
		int width = 0;

	reswitch:
		switch (ch = *(uchar *) fmt++) {
		case '*':
		case 'h':
		case 'l':
		case 'L':
		case 'j':
		case 't':
		case 'z':
		case 'q':
			flag[ch]++;
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
		case '0':
			for (width = 0;; ++fmt) {
				width = width * 10 + ch - '0';
				ch = *fmt;
				if (ch < '0' || ch > '9')
					break;
			}
			goto reswitch;

		// character
		case '%':
			skipspace(fin);
			if ((c = fgetc(fin)) != '%') {
				ungetc(c, fin);
				return nout;
			}
			break;
		
		case 'n':
			*va_arg(ap, int*) = ftell(fin) - off0;
			nout++;
			break;
			
		case 'd':
		case 'u':
			// optionally signed decimal
			sign = 1;
			c = fgetc(fin);
			if (c == '-'){
				sign = -1;
				c = fgetc(fin);
			}else if (c == '+'){
				sign = 1;
				c = fgetc(fin);
			}
			if (c < '0' || '9' < c) {
				ungetc(c, fin);
				return nout;
			}
		decimal:
			ull = 0;
			while ('0' <= c && c <= '9') {
				ull = 10 * ull + c - '0';
				c = fgetc(fin);
			}
			ungetc(c, fin);
		assign:
			if(sign < 0)
				ull = -ull;
			if(flag['h'] == 2)
				*va_arg(ap, char*) = ull;
			else if(flag['h'] == 1)
				*va_arg(ap, short*) = ull;
			else if(flag['l'] == 1)
				*va_arg(ap, long*) = ull;
			else if(flag['l'] == 2)
				*va_arg(ap, long long*) = ull;
			else
				*va_arg(ap, int*) = ull;
			nout++;
			break;
		
		case 'i':
			// optionally signed integer
			c = fgetc(fin);
			sign = 1;
			if (c == '-'){
				sign = -1;
				c = fgetc(fin);
			}else if (c == '+'){
				sign = 1;
				c = fgetc(fin);
			}
			if (c < '0' || '9' < c) {
				ungetc(c, fin);
				return nout;
			}
			if (c == '0') {
				c = fgetc(fin);
				if (c == 'x') {
					c = fgetc(fin);
					if (('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F'))
						goto hex;
					return nout;
				}
				if ('0' <= c && c <= '7')
					goto octal;
			}
			goto decimal;
		
		case 'o':
			// octal
			sign = 1;
			c = fgetc(fin);
			if (c < '0' || '7' < c) {
				ungetc(c, fin);
				return nout;
			}
		octal:
			ull = 0;
			while ('0' <= c && c <= '7') {
				ull = 8 * ull + c - '0';
				c = fgetc(fin);
			}
			ungetc(c, fin);
			goto assign;

		case 'x':
		case 'X':
			// optionally signed integer
		case 'p':
			// pointer value
			sign = 1;
			c = fgetc(fin);
			if (c == '-'){
				sign = -1;
				c = fgetc(fin);
			}else if (c == '+'){
				sign = 1;
				c = fgetc(fin);
			}
			if ((c < '0' || '9' < c) && (c < 'a' || 'z' < c) && (c < 'A' || 'Z' < c)) {
				ungetc(c, fin);
				return nout;
			}
		hex:
			ull = 0;
			for (;; c = fgetc(fin)) {
				if ('0' <= c && c <= '9')
					ull = 16 * ull + c - '0';
				else if ('a' <= c && c <= 'f')
					ull = 16 * ull + c - 'a' + 10;
				else if ('A' <= c && c <= 'F')
					ull = 16 * ull + c - 'A' + 10;
				else
					break;
			}
			ungetc(c, fin);
			goto assign;
			
		case 'a':
		case 'A':
		case 'e':
		case 'E':
		case 'f':
		case 'F':
		case 'g':
		case 'G':
			// float in style of strtod

		case 's':
			// string

		case 'S':
			// long string
		
		case 'c':
			// fixed # chars
		
		case 'C':
			// same as lc
		
		case '[':
			// nonempty sequence of chars from specified char set
			
			printf("sscanf only partially implemented\n");
			abort();
		
		default:
			printf("unrecognized verb %c (%d)\n", ch, ch);
			abort();
		}
	}
}


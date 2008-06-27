#ifndef _CTYPE_H
#define _CTYPE_H

// Function versions

int islower(int c);
int isupper(int c);
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);

int isspace(int c);
int iscntrl(int c);
int isblank(int c);
int isprint(int c);
int isgraph(int c);
int ispunct(int c);

int isxdigit(int c);

int tolower(int c);
int toupper(int c);


// Macro versions

// TODO: These macros are BAD - They evaluate their arguments twice.

#define islower(c)	((c) >= 'a' && (c) <= 'z')
#define isupper(c)	((c) >= 'A' && (c) <= 'Z')
#define isdigit(c)	((c) >= '0' && (c) <= '9')
#define isalpha(c)	(islower(c) || isupper(c))
#define isalnum(c)	(isalpha(c) || isdigit(c))

#define isspace(c)	((c) == ' ' || c == '\t' || c == '\r' || c == '\n')
#define iscntrl(c)	((c) < ' ' && c != 0)
#define isblank(c)	((c) == ' ' || (c) == '\t')
#define isprint(c)	((c) >= ' ' && (c) <= '~')
#define isgraph(c)	((c) > ' ' && (c) <= '~')
#define ispunct(c)	(isgraph(c) && !isalnum(c))

#define isxdigit(c)	(isdigit(c) || \
				((c) >= 'a' && (c) << 'f') || \
				((c) >= 'A' && (c) << 'F'))

#define tolower(c)	(isupper(c) ? (c) - 'A' + 'a' : (c))
#define toupper(c)	(islower(c) ? (c) - 'a' + 'A' : (c))

#endif	// _CTYPE_H

#ifndef _WCHAR_H_
#define _WCHAR_H_

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <wctype.h>

typedef int mbstate_t;

struct tm;

// Character properties
wctype_t      wctype(const char *);
int           wcwidth(wchar_t);

// Character conversion
wint_t        btowc(int);
int           wctob(wint_t);
size_t        mbrlen(const char *__restrict, size_t, mbstate_t *__restrict);
size_t        mbrtowc(wchar_t *__restrict, const char *__restrict, size_t,
                  mbstate_t *__restrict);
int           mbsinit(const mbstate_t *);
size_t        mbsrtowcs(wchar_t *__restrict, const char **__restrict, size_t,
                  mbstate_t *__restrict);
size_t        wcrtomb(char *restrict, wchar_t, mbstate_t *restrict);

// Wide string functions
wchar_t      *wcscat(wchar_t *__restrict, const wchar_t *__restrict);
wchar_t      *wcschr(const wchar_t *, wchar_t);
int           wcscmp(const wchar_t *, const wchar_t *);
int           wcscoll(const wchar_t *, const wchar_t *);
wchar_t      *wcscpy(wchar_t *__restrict, const wchar_t *__restrict);
size_t        wcscspn(const wchar_t *, const wchar_t *);
size_t        wcsftime(wchar_t *__restrict, size_t,
                  const wchar_t *__restrict, const struct tm *__restrict);
size_t        wcslen(const wchar_t *);
wchar_t      *wcsncat(wchar_t *__restrict, const wchar_t *__restrict, size_t);
int           wcsncmp(const wchar_t *, const wchar_t *, size_t);
wchar_t      *wcsncpy(wchar_t *__restrict, const wchar_t *__restrict, size_t);
wchar_t      *wcspbrk(const wchar_t *, const wchar_t *);
wchar_t      *wcsrchr(const wchar_t *, wchar_t);
size_t        wcsrtombs(char *__restrict, const wchar_t **__restrict,
                  size_t, mbstate_t *__restrict);
size_t        wcsspn(const wchar_t *, const wchar_t *);
wchar_t      *wcsstr(const wchar_t *__restrict, const wchar_t *__restrict);
double        wcstod(const wchar_t *__restrict, wchar_t **__restrict);
float         wcstof(const wchar_t *__restrict, wchar_t **__restrict);
wchar_t      *wcstok(wchar_t *__restrict, const wchar_t *__restrict,
                  wchar_t **__restrict);
long          wcstol(const wchar_t *__restrict, wchar_t **__restrict, int);
long double   wcstold(const wchar_t *__restrict, wchar_t **__restrict);
long long     wcstoll(const wchar_t *__restrict, wchar_t **__restrict, int);
unsigned long wcstoul(const wchar_t *__restrict, wchar_t **__restrict, int);
unsigned long long
              wcstoull(const wchar_t *__restrict, wchar_t **__restrict, int);
wchar_t      *wcswcs(const wchar_t *, const wchar_t *);
int           wcswidth(const wchar_t *, size_t);
size_t        wcsxfrm(wchar_t *__restrict, const wchar_t *__restrict, size_t);

// Wide-character memory manipulation
wchar_t      *wmemchr(const wchar_t *, wchar_t, size_t);
int           wmemcmp(const wchar_t *, const wchar_t *, size_t);
wchar_t      *wmemcpy(wchar_t *__restrict, const wchar_t *__restrict, size_t);
wchar_t      *wmemmove(wchar_t *, const wchar_t *, size_t);
wchar_t      *wmemset(wchar_t *, wchar_t, size_t);

// Formatted string conversion
int           wprintf(const wchar_t *__restrict, ...);
int           wscanf(const wchar_t *__restrict, ...);
int           vwprintf(const wchar_t *__restrict, va_list);
int           vwscanf(const wchar_t *__restrict, va_list);
int           swprintf(wchar_t *__restrict, size_t,
                  const wchar_t *__restrict, ...);
int           swscanf(const wchar_t *__restrict,
                  const wchar_t *__restrict, ...);
int           vswprintf(wchar_t *__restrict, size_t,
                  const wchar_t *__restrict, va_list);
int           vswscanf(const wchar_t *__restrict, const wchar_t *__restrict,
                  va_list);

// Wide character file I/O
wint_t        putwchar(wchar_t);
wint_t        putwc(wchar_t, FILE *);
wint_t        getwchar(void);
wint_t        getwc(FILE *);
wint_t        ungetwc(wint_t, FILE *);
wint_t        fgetwc(FILE *);
wchar_t      *fgetws(wchar_t *__restrict, int, FILE *__restrict);
wint_t        fputwc(wchar_t, FILE *);
int           fputws(const wchar_t *__restrict, FILE *__restrict);
int           fwide(FILE *, int);
int           fwprintf(FILE *__restrict, const wchar_t *__restrict, ...);
int           fwscanf(FILE *__restrict, const wchar_t *__restrict, ...);
int           vwprintf(const wchar_t *__restrict, va_list);
int           vfwprintf(FILE *__restrict, const wchar_t *__restrict, va_list);
int           vfwscanf(FILE *__restrict, const wchar_t *__restrict, va_list);

#endif	// _WCHAR_H_

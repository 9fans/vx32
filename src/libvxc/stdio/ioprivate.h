#ifndef STDIO_IOPRIVATE_H
#define STDIO_IOPRIVATE_H

#include <stdio.h>
#include <stdarg.h>

// Buffered I/O helpers
int __writebuf(FILE *f, const void *buf, size_t size);
int __getospace(FILE *f);
int __getinput(FILE *f);

// Formatted I/O helpers
int printfmt(int (*putch)(int, void*), void *putdat, const char *fmt, ...);
int vprintfmt(int (*putch)(int, void*), void *putdat, const char *fmt,
		va_list ap);

#endif	// STDIO_IOPRIVATE_H

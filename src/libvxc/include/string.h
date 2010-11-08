#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

// Simple string functions
char *strcat(char *s, const char *append);
char *strchr(const char *p, int ch);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *to, const char *from);
size_t strcspn(const char *s1, const char *s2);
char *strdup(const char *str);
size_t strlen(const char *str);
char *strncat(char *dst, const char *src, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);
char *strncpy(char *dst, const char *src, size_t n);
char *strpbrk(const char *s1, const char *s2);
char *strrchr(const char *p, int ch);
size_t strspn(const char *s1, const char *s2);
char *strstr(const char *s, const char *find);
char *strtok(char *s, const char *delim);
char *strtok_r(char *s, const char *delim, char **last);

// Simple memory functions
void *memcpy(void *dst, const void *src, size_t size);
void *memmove(void *dst, const void *src, size_t size);
void *memccpy(void *t, const void *f, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memset(void *dst, int ch, size_t size);
void *memchr(const void *s, int c, size_t n);

// Error strings
char *strerror(int);
extern const char *const sys_errlist[];
extern const int sys_nerr;

// Locale-sensitive string functions
int strcoll(const char *s1, const char *s2);
size_t strxfrm(char *__restrict s1, const char *__restrict s2, size_t n);

size_t strlcpy(char *dst, const char *src, size_t size);
size_t strlcat(char *dst, const char *src, size_t size);

#endif	// _STRING_H

#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>


#define RAND_MAX	2147483646	// rand() returns values mod 2^31-1


typedef struct {
	int quot;			// quotient
	int rem;			// remainder
} div_t;
typedef struct {
	long quot;			// quotient
	long rem;			// remainder
} ldiv_t;
typedef struct {
	long long quot;			// quotient
	long long rem;			// remainder
} lldiv_t;


// Process exit
void exit(int status);
void abort(void);
int atexit(void (*func)(void));

// Memory allocation
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *memalign(size_t boundary, size_t size);
void free(void *ptr);

// Numeric functions
int abs(int i);
long labs(long i);
long long llabs(long long i);

div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

// Numeric conversion
int atoi(const char *str);
long atol(const char *str);
long long atoll(const char *str);
double atof(const char *str);
long strtol(const char *__restrict str,
		char **__restrict endptr, int base);
long long strtoll(const char *__restrict str,
		char **__restrict endptr, int base);
unsigned long strtoul(const char *__restrict str,
		char **__restrict endptr, int base);
unsigned long long strtoull(const char *__restrict str,
		char **__restrict endptr, int base);
float strtof(const char *__restrict nptr, char **__restrict endptr);;
double strtod(const char *__restrict nptr, char **__restrict endptr);

// Multibyte string conversion functions
int mblen(const char *, size_t);
int mbtowc(wchar_t *__restrict, const char *__restrict, size_t);
size_t mbstowcs(wchar_t *__restrict, const char *__restrict, size_t);

// Sorting and searching
void qsort(void *base, size_t nelt, size_t eltsize,
		int (*cmp)(const void *a, const void *b));
void *bsearch(const void *key, const void *base, size_t nelt,
		size_t eltsize, int (*cmp)(const void *a, const void *b));

// Environment variables
char *getenv(const char *name);
int setenv(const char *envname, const char *envval, int overwrite);
int unsetenv(const char *name);
int putenv(const char *string);

// Temporary files
char *mktemp(char *tmpl);
int mkstemp(char *tmpl);

// Random numbers
int rand(void);
int rand_r(unsigned *seed);
void srand(unsigned seed);

double drand48(void);
long lrand48(void);
long mrand48(void);
double erand48(unsigned short xsubi[3]);
long nrand48(unsigned short xsubi[3]);
long jrand48(unsigned short xsubi[3]);
void srand48(long seedval);
void lcong48(unsigned short param[7]);
unsigned short *seed48(unsigned short seed16v[3]);

// System commands
int system(const char *command);

void	abort(void);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#endif	// STDLIB_H

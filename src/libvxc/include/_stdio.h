#ifndef _STDIO_H
#define _STDIO_H

#include <sys/types.h>
#include <stddef.h>
#include <stdarg.h>


typedef struct {
	int fd;			// Underlying file descriptor

	unsigned char bufmode;	// Buffering mode
	unsigned char eofflag;	// End-of-file indicator
	unsigned char errflag;	// Error indicator

	unsigned char *obuf;	// Output buffer
	unsigned opos;		// Write position in output buffer
	unsigned omax;		// Size of output buffer

	unsigned char *ibuf;	// Input buffer
	unsigned ipos;		// Read position in input buffer
	unsigned ilim;		// Read limit in input buffer
	unsigned imax;		// Size of input buffer

	int append;		// Always append to end of file
	int isstring;		// is constant string
	unsigned ioffset;	// offset of last read
} FILE;

typedef signed long long fpos_t;

#define EOF		(-1)	// End-of-file return value.

#ifndef SEEK_SET
#define SEEK_SET	0	// Seek relative to start-of-file.
#define SEEK_CUR	1	// Seek relative to current position.
#define SEEK_END	2	// Seek relative to end-of-file.
#endif

#define BUFSIZ		1024	// Standard I/O buffer size

#define _IOFBF		0	// Input/output fully buffered.
#define _IOLBF		1	// Input/output line buffered.
#define _IONBF		2	// Input/output unbuffered.


// Standard I/O streams
extern FILE __stdin, __stdout, __stderr;
#define stdin (&__stdin)
#define stdout (&__stdout)
#define stderr (&__stderr)


#define L_tmpnam	20


// File handling
FILE *fopen(const char *path, const char *mode);
FILE *fdopen(int fildes, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *stream);
int fclose(FILE *fp);

// Temporary files
FILE *tmpfile(void);
char *tmpnam(char *buf);

// Character output
int fputc(int c, FILE *f);
int putc(int c, FILE *f);
int putchar(int c);
#define putc(c,f)	((f)->opos < (f)->omax && (c) != '\n' \
				? (int)((f)->obuf[(f)->opos++] = (c)) \
				: fputc(c,f))
#define putchar(c)	putc(c, stdout)

// Unformatted output
int fputs(const char *s, FILE *f);
int puts(const char *s);
size_t fwrite(const void *__restrict buf, size_t eltsize, size_t nelts,
		FILE *__restrict f);

// Formatted output
int printf(const char *format, ...);
int fprintf(FILE *f, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, int n, const char *fmt, ...);

int vprintf(const char *format, va_list ap);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int vsprintf(char *buf, const char *fmt, va_list ap);
int vsnprintf(char *buf, int n, const char *fmt, va_list ap);

// Character input
int fgetc(FILE *f);
int getc(FILE *f);
int getchar(void);
int ungetc(int c, FILE *f);
#define getc(f)		((f)->ipos < (f)->ilim \
				? (int)((f)->ibuf[(f)->ipos++]) \
				: fgetc(f))
#define getchar()	getc(stdin)
int ungetc(int, FILE*);

// Unformatted input
char *gets(char *s);
char *fgets(char *s, int size, FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

// Formatted input
int scanf(const char *__restrict format, ... );
int fscanf(FILE *__restrict f, const char *__restrict format, ... );
int sscanf(const char *__restrict str, const char *__restrict format, ... );

int vscanf(const char *__restrict format, va_list);
int vfscanf(FILE *__restrict f, const char *__restrict format, va_list);
int vsscanf(const char *__restrict str, const char *__restrict format, va_list);

// Seek position
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
int fgetpos(FILE *__restrict f, fpos_t *__restrict pos);
int fsetpos(FILE *f, const fpos_t *pos);

off_t ftello(FILE *stream);

// Error/EOF handling
int feof(FILE *f);
int ferror(FILE *f);
void clearerr(FILE *f);
#define feof(f)		((int)(f)->eofflag)
#define ferror(f)	((int)(f)->errflag)
#define clearerror(f)	((void)((f)->errflag = 0))

// Buffer management
int fflush(FILE *f);
void setbuf(FILE *__restrict f, char *__restrict buf);
int setvbuf(FILE *__restrict f, char *__restrict buf, int type, size_t size);

// Misc
#define fileno(f)	((f)->fd)

void perror(const char*);
int remove(const char*);

// File management
int remove(const char *path);
int rename(const char *from, const char *to);

#define FILENAME_MAX 1024

#endif	// _STDIO_H

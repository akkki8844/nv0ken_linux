#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define BUFSIZ   4096

FILE  *fopen(const char *path, const char *mode);
FILE  *fdopen(int fd, const char *mode);
int    fclose(FILE *f);
int    fflush(FILE *f);
size_t fread(void *buf, size_t size, size_t nmemb, FILE *f);
size_t fwrite(const void *buf, size_t size, size_t nmemb, FILE *f);
int    fseek(FILE *f, long offset, int whence);
long   ftell(FILE *f);
void   rewind(FILE *f);
int    feof(FILE *f);
int    ferror(FILE *f);
int    fileno(FILE *f);

int    fgetc(FILE *f);
int    fputc(int c, FILE *f);
char  *fgets(char *buf, int n, FILE *f);
int    fputs(const char *s, FILE *f);
int    getc(FILE *f);
int    putc(int c, FILE *f);
int    getchar(void);
int    putchar(int c);
int    puts(const char *s);

int    printf(const char *fmt, ...);
int    fprintf(FILE *f, const char *fmt, ...);
int    sprintf(char *buf, const char *fmt, ...);
int    snprintf(char *buf, size_t size, const char *fmt, ...);
int    vprintf(const char *fmt, va_list ap);
int    vfprintf(FILE *f, const char *fmt, va_list ap);
int    vsprintf(char *buf, const char *fmt, va_list ap);
int    vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

int    scanf(const char *fmt, ...);
int    fscanf(FILE *f, const char *fmt, ...);
int    sscanf(const char *str, const char *fmt, ...);

void   perror(const char *msg);
int    remove(const char *path);
int    rename(const char *old, const char *new);

#endif
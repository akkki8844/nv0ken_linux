#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

void  *malloc(size_t size);
void  *calloc(size_t nmemb, size_t size);
void  *realloc(void *ptr, size_t size);
void   free(void *ptr);

void   exit(int status);
void   _exit(int status);
void   abort(void);
int    atexit(void (*fn)(void));

int    atoi(const char *s);
long   atol(const char *s);
long long atoll(const char *s);
double atof(const char *s);

long      strtol(const char *s, char **endptr, int base);
long long strtoll(const char *s, char **endptr, int base);
unsigned long      strtoul(const char *s, char **endptr, int base);
unsigned long long strtoull(const char *s, char **endptr, int base);
double strtod(const char *s, char **endptr);

int    abs(int x);
long   labs(long x);
long long llabs(long long x);

int    rand(void);
void   srand(unsigned int seed);
#define RAND_MAX 2147483647

void  *bsearch(const void *key, const void *base, size_t nmemb,
               size_t size, int (*cmp)(const void *, const void *));
void   qsort(void *base, size_t nmemb, size_t size,
             int (*cmp)(const void *, const void *));

char  *getenv(const char *name);
int    setenv(const char *name, const char *value, int overwrite);
int    unsetenv(const char *name);
int    putenv(char *string);

int    system(const char *cmd);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#endif
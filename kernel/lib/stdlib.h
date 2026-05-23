#ifndef NV0KEN_LIB_STDLIB_H
#define NV0KEN_LIB_STDLIB_H

#include <stddef.h>
#include <stdint.h>

int atoi(const char *text);
long atol(const char *text);
unsigned long strtoul(const char *text, char **endptr, int base);
long strtol(const char *text, char **endptr, int base);
void qsort(void *base, size_t count, size_t size,
           int (*compar)(const void *, const void *));

#endif

#ifndef NV0KEN_LIB_KPRINTF_H
#define NV0KEN_LIB_KPRINTF_H

#include <stdarg.h>

void kputchar(char ch);
void kputs(const char *text);
int kvprintf(const char *fmt, va_list args);
int kprintf(const char *fmt, ...);

#endif

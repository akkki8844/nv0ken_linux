#include "panic.h"

#include <stdarg.h>

#include "lib/kprintf.h"

void panic(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    kputs("kernel panic: ");
    kvprintf(fmt, args);
    kputchar('\n');
    va_end(args);

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

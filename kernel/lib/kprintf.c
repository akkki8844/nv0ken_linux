#include "kprintf.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/framebuffer.h"
#include "drivers/serial.h"
#include "klog.h"

void kputchar(char ch)
{
    klog_putc(ch);
    serial_write_char(ch);
    framebuffer_putc(ch);
}

void kputs(const char *text)
{
    if (!text) {
        text = "(null)";
    }

    while (*text) {
        kputchar(*text++);
    }
}

static int emit_string(const char *text)
{
    int count = 0;

    if (!text) {
        text = "(null)";
    }

    while (*text) {
        kputchar(*text++);
        ++count;
    }

    return count;
}

static int emit_unsigned(uint64_t value, unsigned base, bool uppercase)
{
    char digits[] = "0123456789abcdef";
    char buffer[32];
    int pos = 0;
    int count = 0;

    if (uppercase) {
        digits[10] = 'A';
        digits[11] = 'B';
        digits[12] = 'C';
        digits[13] = 'D';
        digits[14] = 'E';
        digits[15] = 'F';
    }

    if (value == 0) {
        kputchar('0');
        return 1;
    }

    while (value > 0) {
        buffer[pos++] = digits[value % base];
        value /= base;
    }

    while (pos > 0) {
        kputchar(buffer[--pos]);
        ++count;
    }

    return count;
}

static int emit_signed(int64_t value)
{
    if (value < 0) {
        kputchar('-');
        return 1 + emit_unsigned((uint64_t)-value, 10, false);
    }

    return emit_unsigned((uint64_t)value, 10, false);
}

int kvprintf(const char *fmt, va_list args)
{
    int count = 0;

    while (fmt && *fmt) {
        if (*fmt != '%') {
            kputchar(*fmt++);
            ++count;
            continue;
        }

        ++fmt;
        enum { LENGTH_DEFAULT, LENGTH_LONG, LENGTH_LONG_LONG, LENGTH_SIZE } length = LENGTH_DEFAULT;
        if (*fmt == 'z') {
            length = LENGTH_SIZE;
            ++fmt;
        } else if (*fmt == 'l') {
            length = LENGTH_LONG;
            ++fmt;
            if (*fmt == 'l') {
                length = LENGTH_LONG_LONG;
                ++fmt;
            }
        }

        char specifier = *fmt++;
        switch (specifier) {
        case '%':
            kputchar('%');
            ++count;
            break;
        case 'c':
            kputchar((char)va_arg(args, int));
            ++count;
            break;
        case 's':
            count += emit_string(va_arg(args, const char *));
            break;
        case 'd':
        case 'i':
            if (length == LENGTH_LONG_LONG) {
                count += emit_signed(va_arg(args, long long));
            } else if (length == LENGTH_LONG || length == LENGTH_SIZE) {
                count += emit_signed(va_arg(args, long));
            } else {
                count += emit_signed(va_arg(args, int));
            }
            break;
        case 'u':
            if (length == LENGTH_LONG_LONG) {
                count += emit_unsigned(va_arg(args, unsigned long long), 10, false);
            } else if (length == LENGTH_LONG || length == LENGTH_SIZE) {
                count += emit_unsigned(va_arg(args, unsigned long), 10, false);
            } else {
                count += emit_unsigned(va_arg(args, unsigned int), 10, false);
            }
            break;
        case 'x':
            if (length == LENGTH_LONG_LONG) {
                count += emit_unsigned(va_arg(args, unsigned long long), 16, false);
            } else if (length == LENGTH_LONG || length == LENGTH_SIZE) {
                count += emit_unsigned(va_arg(args, unsigned long), 16, false);
            } else {
                count += emit_unsigned(va_arg(args, unsigned int), 16, false);
            }
            break;
        case 'X':
            if (length == LENGTH_LONG_LONG) {
                count += emit_unsigned(va_arg(args, unsigned long long), 16, true);
            } else if (length == LENGTH_LONG || length == LENGTH_SIZE) {
                count += emit_unsigned(va_arg(args, unsigned long), 16, true);
            } else {
                count += emit_unsigned(va_arg(args, unsigned int), 16, true);
            }
            break;
        case 'p':
            count += emit_string("0x");
            count += emit_unsigned((uintptr_t)va_arg(args, void *), 16, false);
            break;
        default:
            kputchar('?');
            ++count;
            break;
        }
    }

    return count;
}

int kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int count = kvprintf(fmt, args);
    va_end(args);
    return count;
}

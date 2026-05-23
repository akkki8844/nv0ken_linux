#include "stdlib.h"

static int digit_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'z') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A' + 10;
    }
    return -1;
}

static const char *skip_space(const char *text)
{
    while (*text == ' ' || *text == '\t' || *text == '\n' ||
           *text == '\r' || *text == '\v' || *text == '\f') {
        ++text;
    }
    return text;
}

unsigned long strtoul(const char *text, char **endptr, int base)
{
    const char *p = skip_space(text ? text : "");
    unsigned long value = 0;

    if (base == 0) {
        base = 10;
        if (p[0] == '0') {
            base = 8;
            if (p[1] == 'x' || p[1] == 'X') {
                base = 16;
                p += 2;
            }
        }
    } else if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }

    const char *start = p;
    for (;;) {
        int digit = digit_value(*p);
        if (digit < 0 || digit >= base) {
            break;
        }
        value = value * (unsigned long)base + (unsigned long)digit;
        ++p;
    }

    if (endptr) {
        *endptr = (char *)(p == start ? text : p);
    }
    return value;
}

long strtol(const char *text, char **endptr, int base)
{
    const char *p = skip_space(text ? text : "");
    int negative = 0;

    if (*p == '+' || *p == '-') {
        negative = (*p == '-');
        ++p;
    }

    unsigned long value = strtoul(p, endptr, base);
    if (endptr && *endptr == p) {
        *endptr = (char *)text;
    }
    return negative ? -(long)value : (long)value;
}

int atoi(const char *text)
{
    return (int)strtol(text, 0, 10);
}

long atol(const char *text)
{
    return strtol(text, 0, 10);
}

static void swap_bytes(unsigned char *left, unsigned char *right, size_t size)
{
    while (size--) {
        unsigned char tmp = *left;
        *left++ = *right;
        *right++ = tmp;
    }
}

void qsort(void *base, size_t count, size_t size,
           int (*compar)(const void *, const void *))
{
    if (!base || count < 2 || size == 0 || !compar) {
        return;
    }

    unsigned char *bytes = base;
    for (size_t i = 1; i < count; ++i) {
        size_t j = i;
        while (j > 0) {
            unsigned char *a = bytes + (j - 1) * size;
            unsigned char *b = bytes + j * size;
            if (compar(a, b) <= 0) {
                break;
            }
            swap_bytes(a, b, size);
            --j;
        }
    }
}

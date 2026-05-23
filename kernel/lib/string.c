#include "string.h"

void *memset(void *dest, int value, size_t count)
{
    unsigned char *out = dest;
    while (count--) {
        *out++ = (unsigned char)value;
    }
    return dest;
}

void *memcpy(void *dest, const void *src, size_t count)
{
    unsigned char *out = dest;
    const unsigned char *in = src;
    while (count--) {
        *out++ = *in++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t count)
{
    unsigned char *out = dest;
    const unsigned char *in = src;

    if (out < in) {
        while (count--) {
            *out++ = *in++;
        }
    } else if (out > in) {
        out += count;
        in += count;
        while (count--) {
            *--out = *--in;
        }
    }

    return dest;
}

int memcmp(const void *lhs, const void *rhs, size_t count)
{
    const unsigned char *left = lhs;
    const unsigned char *right = rhs;

    while (count--) {
        if (*left != *right) {
            return (int)*left - (int)*right;
        }
        ++left;
        ++right;
    }

    return 0;
}

size_t strlen(const char *text)
{
    size_t length = 0;
    while (text && text[length]) {
        ++length;
    }
    return length;
}

int strcmp(const char *lhs, const char *rhs)
{
    while (*lhs && *lhs == *rhs) {
        ++lhs;
        ++rhs;
    }
    return (unsigned char)*lhs - (unsigned char)*rhs;
}

char *strcpy(char *dest, const char *src)
{
    char *out = dest;
    while ((*out++ = *src++) != '\0') {
    }
    return dest;
}

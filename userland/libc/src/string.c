#include <string.h>
#include <stdlib.h>
#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char       *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char       *d = dst;
    const unsigned char *s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = a, *pb = b;
    while (n--) {
        if (*pa != *pb) return *pa - *pb;
        pa++; pb++;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    while (n--) {
        if (*p == (unsigned char)c) return (void *)p;
        p++;
    }
    return 0;
}

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return p - s;
}

size_t strnlen(const char *s, size_t maxlen) {
    size_t n = 0;
    while (n < maxlen && s[n]) n++;
    return n;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dst;
}

char *strcat(char *dst, const char *src) {
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++));
    return dst;
}

char *strncat(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (*d) d++;
    while (n-- && (*d++ = *src++));
    if (!*(d-1)) {}
    else *d = '\0';
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && *a == *b) { a++; b++; }
    if (n == (size_t)-1) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

static int to_lower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

int strcasecmp(const char *a, const char *b) {
    while (*a && to_lower(*a) == to_lower(*b)) { a++; b++; }
    return to_lower((unsigned char)*a) - to_lower((unsigned char)*b);
}

int strncasecmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && to_lower(*a) == to_lower(*b)) { a++; b++; }
    if (n == (size_t)-1) return 0;
    return to_lower((unsigned char)*a) - to_lower((unsigned char)*b);
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return c == '\0' ? (char *)s : 0;
}

char *strrchr(const char *s, int c) {
    const char *last = 0;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (*haystack == *needle && memcmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
        haystack++;
    }
    return 0;
}

char *strtok(char *s, const char *delim) {
    static char *saved = 0;
    return strtok_r(s, delim, &saved);
}

char *strtok_r(char *s, const char *delim, char **saveptr) {
    if (s) *saveptr = s;
    if (!*saveptr) return 0;
    s = *saveptr + strspn(*saveptr, delim);
    if (!*s) { *saveptr = 0; return 0; }
    char *end = s + strcspn(s, delim);
    if (*end) { *end = '\0'; *saveptr = end + 1; }
    else        *saveptr = 0;
    return s;
}

char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *out = malloc(len);
    if (!out) return 0;
    memcpy(out, s, len);
    return out;
}

char *strndup(const char *s, size_t n) {
    size_t len = strnlen(s, n);
    char *out = malloc(len + 1);
    if (!out) return 0;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static const char *const error_strings[] = {
    "success",
    "operation not permitted",
    "no such file or directory",
    "no such process",
    "interrupted system call",
    "input/output error",
    "no such device or address",
    "argument list too long",
    "exec format error",
    "bad file descriptor",
    "no child processes",
    "resource temporarily unavailable",
    "out of memory",
    "permission denied",
    "bad address",
    "block device required",
    "device or resource busy",
    "file exists",
    "invalid cross-device link",
    "no such device",
    "not a directory",
    "is a directory",
    "invalid argument",
    "too many open files in system",
    "too many open files",
    "inappropriate ioctl for device",
    "text file busy",
    "file too large",
    "no space left on device",
    "illegal seek",
    "read-only file system",
    "too many links",
    "broken pipe",
    "numerical argument out of domain",
    "numerical result out of range",
    "resource deadlock avoided",
    "file name too long",
    "no locks available",
    "function not implemented",
    "directory not empty",
    "too many levels of symbolic links",
};

char *strerror(int errnum) {
    static char buf[32];
    int max = (int)(sizeof(error_strings) / sizeof(error_strings[0]));
    if (errnum >= 0 && errnum < max)
        return (char *)error_strings[errnum];
    int n = errnum, i = 0;
    char tmp[16];
    if (n < 0) n = -n;
    do { tmp[i++] = '0' + n % 10; n /= 10; } while (n);
    const char *prefix = "unknown error ";
    int plen = 14;
    memcpy(buf, prefix, plen);
    for (int j = 0; j < i; j++) buf[plen + j] = tmp[i - 1 - j];
    buf[plen + i] = '\0';
    return buf;
}

size_t strspn(const char *s, const char *accept) {
    size_t n = 0;
    while (*s && strchr(accept, *s)) { s++; n++; }
    return n;
}

size_t strcspn(const char *s, const char *reject) {
    size_t n = 0;
    while (*s && !strchr(reject, *s)) { s++; n++; }
    return n;
}

char *strpbrk(const char *s, const char *accept) {
    while (*s) {
        if (strchr(accept, *s)) return (char *)s;
        s++;
    }
    return 0;
}
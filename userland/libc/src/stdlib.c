#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* -----------------------------------------------------------------------
 * Heap — sbrk-based linked list allocator
 * --------------------------------------------------------------------- */

typedef struct Block {
    size_t        size;
    int           free;
    struct Block *next;
} Block;

#define BLOCK_META sizeof(Block)
#define ALIGN       16
#define ALIGN_UP(n) (((n) + ALIGN - 1) & ~(size_t)(ALIGN - 1))

static Block *heap_head = 0;

static Block *find_free(size_t size) {
    Block *b = heap_head;
    while (b) {
        if (b->free && b->size >= size) return b;
        b = b->next;
    }
    return 0;
}

static Block *extend_heap(size_t size) {
    Block *b = sbrk(0);
    if (sbrk(BLOCK_META + size) == (void *)-1) return 0;
    b->size = size;
    b->free = 0;
    b->next = 0;
    return b;
}

void *malloc(size_t size) {
    if (size == 0) return 0;
    size = ALIGN_UP(size);

    Block *b = find_free(size);
    if (b) {
        b->free = 0;
        return (void *)(b + 1);
    }

    b = extend_heap(size);
    if (!b) { errno = ENOMEM; return 0; }

    if (!heap_head) {
        heap_head = b;
    } else {
        Block *cur = heap_head;
        while (cur->next) cur = cur->next;
        cur->next = b;
    }
    return (void *)(b + 1);
}

void free(void *ptr) {
    if (!ptr) return;
    Block *b = (Block *)ptr - 1;
    b->free = 1;

    Block *cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += BLOCK_META + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    if (nmemb && total / nmemb != size) { errno = ENOMEM; return 0; }
    void *ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (!size) { free(ptr); return 0; }

    Block *b = (Block *)ptr - 1;
    size = ALIGN_UP(size);

    if (b->size >= size) return ptr;

    void *new_ptr = malloc(size);
    if (!new_ptr) return 0;
    memcpy(new_ptr, ptr, b->size < size ? b->size : size);
    free(ptr);
    return new_ptr;
}

/* -----------------------------------------------------------------------
 * Exit
 * --------------------------------------------------------------------- */

static void (*atexit_fns[32])(void);
static int atexit_count = 0;

int atexit(void (*fn)(void)) {
    if (atexit_count >= 32) return -1;
    atexit_fns[atexit_count++] = fn;
    return 0;
}

void exit(int status) {
    for (int i = atexit_count - 1; i >= 0; i--)
        atexit_fns[i]();
    sys_exit(status);
    __builtin_unreachable();
}

void _exit(int status) {
    sys_exit(status);
    __builtin_unreachable();
}

void abort(void) {
    sys_exit(134);
    __builtin_unreachable();
}

/* -----------------------------------------------------------------------
 * Number conversion
 * --------------------------------------------------------------------- */

int atoi(const char *s) {
    return (int)strtol(s, 0, 10);
}

long atol(const char *s) {
    return strtol(s, 0, 10);
}

long long atoll(const char *s) {
    return strtoll(s, 0, 10);
}

double atof(const char *s) {
    return strtod(s, 0);
}

long strtol(const char *s, char **endptr, int base) {
    while (*s == ' ' || *s == '\t') s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    if (base == 0) {
        if (*s == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (*s == '0') { base = 8; s++; }
        else base = 10;
    } else if (base == 16 && *s == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    long val = 0;
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') digit = *s - 'A' + 10;
        else break;
        if (digit >= base) break;
        val = val * base + digit;
        s++;
    }
    if (endptr) *endptr = (char *)s;
    return neg ? -val : val;
}

long long strtoll(const char *s, char **endptr, int base) {
    return (long long)strtol(s, endptr, base);
}

unsigned long strtoul(const char *s, char **endptr, int base) {
    return (unsigned long)strtol(s, endptr, base);
}

unsigned long long strtoull(const char *s, char **endptr, int base) {
    return (unsigned long long)strtol(s, endptr, base);
}

double strtod(const char *s, char **endptr) {
    while (*s == ' ' || *s == '\t') s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    double val = 0.0, frac = 0.1;
    while (*s >= '0' && *s <= '9') { val = val * 10.0 + (*s++ - '0'); }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            val += (*s++ - '0') * frac;
            frac *= 0.1;
        }
    }
    if (endptr) *endptr = (char *)s;
    return neg ? -val : val;
}

/* -----------------------------------------------------------------------
 * Math
 * --------------------------------------------------------------------- */

int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }
long long llabs(long long x) { return x < 0 ? -x : x; }

/* -----------------------------------------------------------------------
 * Random
 * --------------------------------------------------------------------- */

static unsigned int rand_seed = 1;

int rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (int)((rand_seed >> 16) & 0x7FFF);
}

void srand(unsigned int seed) {
    rand_seed = seed;
}

/* -----------------------------------------------------------------------
 * Search and sort
 * --------------------------------------------------------------------- */

void *bsearch(const void *key, const void *base, size_t nmemb,
              size_t size, int (*cmp)(const void *, const void *)) {
    size_t lo = 0, hi = nmemb;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const void *elem = (const char *)base + mid * size;
        int r = cmp(key, elem);
        if (r == 0) return (void *)elem;
        if (r  < 0) hi = mid;
        else        lo = mid + 1;
    }
    return 0;
}

static void swap_bytes(void *a, void *b, size_t size) {
    unsigned char *pa = a, *pb = b, tmp;
    while (size--) { tmp = *pa; *pa++ = *pb; *pb++ = tmp; }
}

static void qsort_r(void *base, size_t lo, size_t hi, size_t size,
                    int (*cmp)(const void *, const void *)) {
    if (lo >= hi) return;
    char *b = base;
    void *pivot = b + hi * size;
    size_t i = lo;
    for (size_t j = lo; j < hi; j++) {
        if (cmp(b + j * size, pivot) <= 0)
            swap_bytes(b + i++ * size, b + j * size, size);
    }
    swap_bytes(b + i * size, b + hi * size, size);
    if (i > lo) qsort_r(base, lo, i - 1, size, cmp);
    qsort_r(base, i + 1, hi, size, cmp);
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*cmp)(const void *, const void *)) {
    if (nmemb > 1) qsort_r(base, 0, nmemb - 1, size, cmp);
}

/* -----------------------------------------------------------------------
 * Environment
 * --------------------------------------------------------------------- */

extern char **environ;

char *getenv(const char *name) {
    if (!environ || !name) return 0;
    size_t nlen = strlen(name);
    for (char **e = environ; *e; e++) {
        if (strncmp(*e, name, nlen) == 0 && (*e)[nlen] == '=')
            return *e + nlen + 1;
    }
    return 0;
}

int setenv(const char *name, const char *value, int overwrite) {
    if (!name || !*name || strchr(name, '=')) { errno = EINVAL; return -1; }
    if (!overwrite && getenv(name)) return 0;
    size_t nlen = strlen(name), vlen = strlen(value);
    char *entry = malloc(nlen + vlen + 2);
    if (!entry) return -1;
    memcpy(entry, name, nlen);
    entry[nlen] = '=';
    memcpy(entry + nlen + 1, value, vlen + 1);
    return putenv(entry);
}

int unsetenv(const char *name) {
    if (!environ || !name) return 0;
    size_t nlen = strlen(name);
    for (char **e = environ; *e; e++) {
        if (strncmp(*e, name, nlen) == 0 && (*e)[nlen] == '=') {
            char **dst = e;
            while (*dst) { *dst = *(dst + 1); dst++; }
            return 0;
        }
    }
    return 0;
}

int putenv(char *string) {
    if (!string || !strchr(string, '=')) { errno = EINVAL; return -1; }
    char *eq = strchr(string, '=');
    size_t nlen = eq - string;
    if (environ) {
        for (char **e = environ; *e; e++) {
            if (strncmp(*e, string, nlen) == 0 && (*e)[nlen] == '=') {
                *e = string;
                return 0;
            }
        }
    }
    return 0;
}

int system(const char *cmd) {
    if (!cmd) return 1;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        char *argv[] = { "/bin/nv0sh", "-c", (char *)cmd, 0 };
        execve("/bin/nv0sh", argv, environ);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return status;
}
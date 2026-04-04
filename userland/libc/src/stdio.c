#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>

struct FILE {
    int   fd;
    int   eof;
    int   error;
    int   mode;
    char *buf;
    int   buf_len;
    int   buf_pos;
    int   buf_dirty;
};

static FILE _stdin_file  = { 0, 0, 0, O_RDONLY, 0, 0, 0, 0 };
static FILE _stdout_file = { 1, 0, 0, O_WRONLY, 0, 0, 0, 0 };
static FILE _stderr_file = { 2, 0, 0, O_WRONLY, 0, 0, 0, 0 };

FILE *stdin  = &_stdin_file;
FILE *stdout = &_stdout_file;
FILE *stderr = &_stderr_file;

FILE *fopen(const char *path, const char *mode) {
    int flags = 0;
    int perm  = 0644;

    if (mode[0] == 'r') {
        flags = mode[1] == '+' ? O_RDWR : O_RDONLY;
    } else if (mode[0] == 'w') {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
        if (mode[1] == '+') flags = O_RDWR | O_CREAT | O_TRUNC;
    } else if (mode[0] == 'a') {
        flags = O_WRONLY | O_CREAT | O_APPEND;
        if (mode[1] == '+') flags = O_RDWR | O_CREAT | O_APPEND;
    } else {
        errno = EINVAL;
        return 0;
    }

    int fd = open(path, flags, perm);
    if (fd < 0) return 0;

    FILE *f = malloc(sizeof(FILE));
    if (!f) { close(fd); return 0; }
    f->fd        = fd;
    f->eof       = 0;
    f->error     = 0;
    f->mode      = flags;
    f->buf       = 0;
    f->buf_len   = 0;
    f->buf_pos   = 0;
    f->buf_dirty = 0;
    return f;
}

FILE *fdopen(int fd, const char *mode) {
    FILE *f = malloc(sizeof(FILE));
    if (!f) return 0;
    int flags = (mode[0] == 'r') ? O_RDONLY :
                (mode[0] == 'w') ? O_WRONLY : O_RDWR;
    f->fd = fd; f->eof = 0; f->error = 0;
    f->mode = flags; f->buf = 0;
    f->buf_len = f->buf_pos = f->buf_dirty = 0;
    return f;
}

int fclose(FILE *f) {
    if (!f) return EOF;
    fflush(f);
    int r = close(f->fd);
    free(f->buf);
    if (f != stdin && f != stdout && f != stderr) free(f);
    return r;
}

int fflush(FILE *f) {
    if (!f || !f->buf_dirty || !f->buf) return 0;
    write(f->fd, f->buf, f->buf_pos);
    f->buf_pos   = 0;
    f->buf_dirty = 0;
    return 0;
}

size_t fread(void *buf, size_t size, size_t nmemb, FILE *f) {
    if (!f || f->eof || f->error) return 0;
    size_t total = size * nmemb;
    if (total == 0) return 0;
    ssize_t n = read(f->fd, buf, total);
    if (n < 0)  { f->error = 1; return 0; }
    if (n == 0) { f->eof   = 1; return 0; }
    return (size_t)n / size;
}

size_t fwrite(const void *buf, size_t size, size_t nmemb, FILE *f) {
    if (!f || f->error) return 0;
    size_t total = size * nmemb;
    if (total == 0) return 0;
    ssize_t n = write(f->fd, buf, total);
    if (n < 0) { f->error = 1; return 0; }
    return (size_t)n / size;
}

int fseek(FILE *f, long offset, int whence) {
    if (!f) return -1;
    fflush(f);
    f->eof = 0;
    return (int)lseek(f->fd, offset, whence);
}

long ftell(FILE *f) {
    if (!f) return -1;
    return (long)lseek(f->fd, 0, SEEK_CUR);
}

void rewind(FILE *f) { if (f) fseek(f, 0, SEEK_SET); }
int  feof(FILE *f)   { return f ? f->eof   : 0; }
int  ferror(FILE *f) { return f ? f->error : 0; }
int  fileno(FILE *f) { return f ? f->fd    : -1; }

int fgetc(FILE *f) {
    unsigned char c;
    if (!f || f->eof || f->error) return EOF;
    ssize_t n = read(f->fd, &c, 1);
    if (n <= 0) { if (n == 0) f->eof = 1; else f->error = 1; return EOF; }
    return (int)c;
}

int fputc(int c, FILE *f) {
    unsigned char ch = (unsigned char)c;
    if (!f || f->error) return EOF;
    if (write(f->fd, &ch, 1) != 1) { f->error = 1; return EOF; }
    return c;
}

char *fgets(char *buf, int n, FILE *f) {
    if (!buf || n <= 0 || !f || f->eof || f->error) return 0;
    int i = 0;
    while (i < n - 1) {
        int c = fgetc(f);
        if (c == EOF) { if (i == 0) return 0; break; }
        buf[i++] = (char)c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return buf;
}

int fputs(const char *s, FILE *f) {
    size_t len = strlen(s);
    return fwrite(s, 1, len, f) == len ? (int)len : EOF;
}

int getc(FILE *f)   { return fgetc(f); }
int putc(int c, FILE *f) { return fputc(c, f); }
int getchar(void)   { return fgetc(stdin); }
int putchar(int c)  { return fputc(c, stdout); }

int puts(const char *s) {
    if (fputs(s, stdout) == EOF) return EOF;
    return fputc('\n', stdout);
}

/* -----------------------------------------------------------------------
 * printf core
 * --------------------------------------------------------------------- */

static void fmt_num(char *buf, int *pos, unsigned long long val,
                    int base, int upper, int width, int zero_pad,
                    int left, int sign_char) {
    char tmp[64];
    int  len = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (val == 0) { tmp[len++] = '0'; }
    else { while (val) { tmp[len++] = digits[val % base]; val /= base; } }

    if (sign_char) tmp[len++] = (char)sign_char;

    int total = len > width ? len : width;
    int pad = total - len;

    if (!left) {
        char pc = zero_pad ? '0' : ' ';
        for (int i = 0; i < pad; i++) buf[(*pos)++] = pc;
    }
    for (int i = len - 1; i >= 0; i--) buf[(*pos)++] = tmp[i];
    if (left) for (int i = 0; i < pad; i++) buf[(*pos)++] = ' ';
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    int pos = 0;
    int max = (int)size - 1;

#define PUT(c) do { if (pos < max) buf[pos] = (c); pos++; } while(0)

    while (*fmt) {
        if (*fmt != '%') { PUT(*fmt++); continue; }
        fmt++;

        int left = 0, zero_pad = 0, plus = 0, space = 0, alt = 0;
        for (;;) {
            if      (*fmt == '-') { left     = 1; fmt++; }
            else if (*fmt == '0') { zero_pad = 1; fmt++; }
            else if (*fmt == '+') { plus     = 1; fmt++; }
            else if (*fmt == ' ') { space    = 1; fmt++; }
            else if (*fmt == '#') { alt      = 1; fmt++; }
            else break;
        }

        int width = 0;
        if (*fmt == '*') { width = va_arg(ap, int); fmt++; }
        else while (*fmt >= '0' && *fmt <= '9') width = width*10 + (*fmt++ - '0');

        int prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') { prec = va_arg(ap, int); fmt++; }
            else while (*fmt >= '0' && *fmt <= '9') prec = prec*10 + (*fmt++ - '0');
        }

        int lng = 0;
        if (*fmt == 'l') { lng = 1; fmt++; if (*fmt == 'l') { lng = 2; fmt++; } }
        else if (*fmt == 'h') { fmt++; }
        else if (*fmt == 'z') { lng = 1; fmt++; }

        char spec = *fmt++;
        char tmp[64];
        int  tlen;

        switch (spec) {
            case 'd': case 'i': {
                long long v = lng == 2 ? va_arg(ap, long long) :
                              lng == 1 ? va_arg(ap, long)      :
                                         va_arg(ap, int);
                int sign = 0;
                unsigned long long uv;
                if (v < 0) { sign = '-'; uv = (unsigned long long)(-v); }
                else { sign = plus ? '+' : space ? ' ' : 0; uv = (unsigned long long)v; }
                fmt_num(tmp, &(tlen = 0), uv, 10, 0, width, zero_pad, left, sign);
                for (int i = 0; i < tlen; i++) PUT(tmp[i]);
                break;
            }
            case 'u': {
                unsigned long long v = lng == 2 ? va_arg(ap, unsigned long long) :
                                       lng == 1 ? va_arg(ap, unsigned long)      :
                                                  va_arg(ap, unsigned int);
                fmt_num(tmp, &(tlen = 0), v, 10, 0, width, zero_pad, left, 0);
                for (int i = 0; i < tlen; i++) PUT(tmp[i]);
                break;
            }
            case 'x': case 'X': {
                unsigned long long v = lng == 2 ? va_arg(ap, unsigned long long) :
                                       lng == 1 ? va_arg(ap, unsigned long)      :
                                                  va_arg(ap, unsigned int);
                if (alt && v) { PUT('0'); PUT(spec == 'X' ? 'X' : 'x'); }
                fmt_num(tmp, &(tlen = 0), v, 16, spec=='X', width, zero_pad, left, 0);
                for (int i = 0; i < tlen; i++) PUT(tmp[i]);
                break;
            }
            case 'o': {
                unsigned long long v = lng == 1 ? va_arg(ap, unsigned long) :
                                                  va_arg(ap, unsigned int);
                fmt_num(tmp, &(tlen = 0), v, 8, 0, width, zero_pad, left, 0);
                for (int i = 0; i < tlen; i++) PUT(tmp[i]);
                break;
            }
            case 'p': {
                unsigned long long v = (unsigned long long)(uintptr_t)va_arg(ap, void *);
                PUT('0'); PUT('x');
                fmt_num(tmp, &(tlen = 0), v, 16, 0, 0, 1, 0, 0);
                for (int i = 0; i < tlen; i++) PUT(tmp[i]);
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                if (!left) for (int i = 1; i < width; i++) PUT(' ');
                PUT(c);
                if (left)  for (int i = 1; i < width; i++) PUT(' ');
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                int slen = (int)strlen(s);
                if (prec >= 0 && slen > prec) slen = prec;
                int pad = width > slen ? width - slen : 0;
                if (!left) for (int i = 0; i < pad; i++) PUT(' ');
                for (int i = 0; i < slen; i++) PUT(s[i]);
                if (left)  for (int i = 0; i < pad; i++) PUT(' ');
                break;
            }
            case 'n': {
                int *n = va_arg(ap, int *);
                if (n) *n = pos;
                break;
            }
            case '%': PUT('%'); break;
            default:  PUT('%'); PUT(spec); break;
        }
    }

#undef PUT
    if (size > 0) buf[pos < max ? pos : max] = '\0';
    return pos;
}

int vprintf(const char *fmt, va_list ap) {
    char buf[4096];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    write(STDOUT_FILENO, buf, n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1);
    return n;
}

int vfprintf(FILE *f, const char *fmt, va_list ap) {
    char buf[4096];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    fwrite(buf, 1, n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1, f);
    return n;
}

int vsprintf(char *buf, const char *fmt, va_list ap) {
    return vsnprintf(buf, 65536, fmt, ap);
}

int printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int fprintf(FILE *f, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vsprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

void perror(const char *msg) {
    if (msg && *msg) { fputs(msg, stderr); fputs(": ", stderr); }
    fputs(strerror(errno), stderr);
    fputc('\n', stderr);
}

int remove(const char *path) { return unlink(path); }

int rename(const char *old, const char *new) {
    return sys_rename(old, new);
}

int scanf(const char *fmt, ...) { (void)fmt; return 0; }
int fscanf(FILE *f, const char *fmt, ...) { (void)f; (void)fmt; return 0; }
int sscanf(const char *str, const char *fmt, ...) { (void)str; (void)fmt; return 0; }
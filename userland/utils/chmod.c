#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static mode_t parse_mode(const char *s, mode_t cur) {
    if (s[0] >= '0' && s[0] <= '7') return (mode_t)strtol(s, NULL, 8);
    mode_t m = cur;
    const char *p = s;
    while (*p) {
        int who = 0;
        while (*p == 'u'||*p=='g'||*p=='o'||*p=='a') {
            if (*p=='u') who|=1; else if(*p=='g') who|=2;
            else if(*p=='o') who|=4; else who|=7;
            p++;
        }
        if (!who) who = 7;
        char op = *p++;
        mode_t bits = 0;
        while (*p=='r'||*p=='w'||*p=='x'||*p=='s'||*p=='t') {
            if (*p=='r') bits|=0444; else if(*p=='w') bits|=0222;
            else if(*p=='x') bits|=0111;
            p++;
        }
        mode_t mask = 0;
        if (who&1) mask |= (bits & 0700);
        if (who&2) mask |= (bits & 0070);
        if (who&4) mask |= (bits & 0007);
        if (op=='+') m |= mask;
        else if(op=='-') m &= ~mask;
        else if(op=='=') m = (m & ~0777) | mask;
        if (*p == ',') p++;
    }
    return m;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "chmod: missing operand\n"); return 1; }
    int ret = 0;
    for (int i = 2; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) < 0) { perror(argv[i]); ret=1; continue; }
        mode_t m = parse_mode(argv[1], st.st_mode);
        if (chmod(argv[i], m) < 0) { perror(argv[i]); ret=1; }
    }
    return ret;
}
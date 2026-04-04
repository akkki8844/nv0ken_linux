#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

static int flag_i=0, flag_v=0, flag_n=0, flag_c=0, flag_l=0, flag_r=0;
static const char *pattern = NULL;

static int str_match(const char *haystack, const char *needle) {
    size_t hl = strlen(haystack), nl = strlen(needle);
    if (nl == 0) return 1;
    for (size_t i = 0; i + nl <= hl; i++) {
        int ok = 1;
        for (size_t j = 0; j < nl; j++) {
            char a = haystack[i+j], b = needle[j];
            if (flag_i) { a=tolower(a); b=tolower(b); }
            if (a != b) { ok=0; break; }
        }
        if (ok) return 1;
    }
    return 0;
}

static int do_grep(const char *path, int show_name) {
    FILE *f = strcmp(path,"-")==0 ? stdin : fopen(path,"r");
    if (!f) { perror(path); return 2; }
    char line[4096];
    long long lnum=0, count=0;
    int matched=0;
    while (fgets(line, sizeof(line), f)) {
        lnum++;
        size_t len = strlen(line);
        if (len>0 && line[len-1]=='\n') line[--len]='\0';
        int m = str_match(line, pattern);
        if (flag_v) m=!m;
        if (!m) continue;
        matched=1; count++;
        if (flag_l) { puts(path); fclose(f); return 0; }
        if (!flag_c) {
            if (show_name) printf("%s:", path);
            if (flag_n) printf("%lld:", lnum);
            puts(line);
        }
    }
    if (flag_c) {
        if (show_name) printf("%s:", path);
        printf("%lld\n", count);
    }
    if (f!=stdin) fclose(f);
    return matched ? 0 : 1;
}

static int do_grep_r(const char *path, int show_name);

int main(int argc, char **argv) {
    int i;
    for (i=1; i<argc && argv[i][0]=='-' && argv[i][1]; i++)
        for (char *f=argv[i]+1; *f; f++) {
            switch(*f) {
                case 'i': flag_i=1; break;
                case 'v': flag_v=1; break;
                case 'n': flag_n=1; break;
                case 'c': flag_c=1; break;
                case 'l': flag_l=1; break;
                case 'r': case 'R': flag_r=1; break;
            }
        }
    if (i>=argc) { fprintf(stderr,"grep: missing pattern\n"); return 2; }
    pattern = argv[i++];
    if (i>=argc) { do_grep("-",0); return 0; }
    int multi = argc-i>1;
    int ret = 0;
    for (; i<argc; i++) ret |= do_grep(argv[i], multi||flag_r);
    return ret;
}
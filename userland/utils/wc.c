#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static int flag_l=0, flag_w=0, flag_c=0;

static void do_wc(const char *path, long long *tl, long long *tw, long long *tc) {
    FILE *f = strcmp(path,"-")==0 ? stdin : fopen(path,"r");
    if (!f) { perror(path); return; }
    long long lines=0, words=0, chars=0;
    int in_word=0, c;
    while ((c=fgetc(f)) != EOF) {
        chars++;
        if (c=='\n') lines++;
        if (c==' '||c=='\t'||c=='\n'||c=='\r') { if (in_word) { words++; in_word=0; } }
        else in_word=1;
    }
    if (in_word) words++;
    if (f!=stdin) fclose(f);
    if (!flag_l && !flag_w && !flag_c) { flag_l=flag_w=flag_c=1; }
    if (flag_l) printf("%8lld", lines);
    if (flag_w) printf("%8lld", words);
    if (flag_c) printf("%8lld", chars);
    printf(" %s\n", path);
    if (tl) *tl+=lines; if (tw) *tw+=words; if (tc) *tc+=chars;
}

int main(int argc, char **argv) {
    int i;
    for (i=1; i<argc && argv[i][0]=='-' && argv[i][1]; i++)
        for (char *f=argv[i]+1; *f; f++) {
            if (*f=='l') flag_l=1;
            else if (*f=='w') flag_w=1;
            else if (*f=='c') flag_c=1;
        }
    if (i>=argc) { do_wc("-",NULL,NULL,NULL); return 0; }
    long long tl=0,tw=0,tc=0;
    int multi = argc-i>1;
    for (; i<argc; i++) do_wc(argv[i], multi?&tl:NULL, multi?&tw:NULL, multi?&tc:NULL);
    if (multi) {
        if (flag_l) printf("%8lld", tl);
        if (flag_w) printf("%8lld", tw);
        if (flag_c) printf("%8lld", tc);
        printf(" total\n");
    }
    return 0;
}
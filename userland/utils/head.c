#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static int nlines = 10;

static int do_head(const char *path) {
    FILE *f = strcmp(path,"-")==0 ? stdin : fopen(path,"r");
    if (!f) { perror(path); return 1; }
    char line[4096];
    for (int i = 0; i < nlines && fgets(line, sizeof(line), f); i++)
        fputs(line, stdout);
    if (f != stdin) fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    int i = 1;
    if (i < argc && argv[i][0]=='-' && argv[i][1]=='n') {
        nlines = atoi(argv[i]+2[0] ? argv[i]+2 : argv[++i]);
        i++;
    } else if (i < argc && argv[i][0]=='-' && argv[i][1]>='1' && argv[i][1]<='9') {
        nlines = atoi(argv[i]+1); i++;
    }
    if (i >= argc) return do_head("-");
    int ret = 0;
    for (; i < argc; i++) {
        if (argc-i > 1) printf("==> %s <==\n", argv[i]);
        ret |= do_head(argv[i]);
    }
    return ret;
}
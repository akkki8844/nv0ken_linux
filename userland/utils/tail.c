#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int nlines = 10;

#define MAX_LINES 65536
#define MAX_LEN   4096

static int do_tail(const char *path) {
    FILE *f = strcmp(path,"-")==0 ? stdin : fopen(path,"r");
    if (!f) { perror(path); return 1; }
    char *lines[MAX_LINES];
    int   count = 0, wrap = 0;
    char  line[MAX_LEN];
    while (fgets(line, sizeof(line), f)) {
        if (count < MAX_LINES) lines[count++] = strdup(line);
        else { free(lines[wrap]); lines[wrap] = strdup(line); wrap=(wrap+1)%MAX_LINES; }
    }
    if (f != stdin) fclose(f);
    int start = count > nlines ? count - nlines : 0;
    for (int i = start; i < count; i++) {
        int idx = (wrap + i) % (count < MAX_LINES ? count : MAX_LINES);
        fputs(lines[idx], stdout);
        free(lines[idx]);
    }
    return 0;
}

int main(int argc, char **argv) {
    int i = 1;
    if (i < argc && argv[i][0]=='-' && argv[i][1]=='n') {
        nlines = atoi(argv[i][2] ? argv[i]+2 : argv[++i]); i++;
    } else if (i < argc && argv[i][0]=='-' && argv[i][1]>='1' && argv[i][1]<='9') {
        nlines = atoi(argv[i]+1); i++;
    }
    if (i >= argc) return do_tail("-");
    int ret = 0;
    for (; i < argc; i++) {
        if (argc-i > 1) printf("==> %s <==\n", argv[i]);
        ret |= do_tail(argv[i]);
    }
    return ret;
}
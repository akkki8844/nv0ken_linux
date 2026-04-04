#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

static int flag_C = 0;

static void dump(const char *path) {
    int fd = strcmp(path,"-")==0 ? STDIN_FILENO : open(path, O_RDONLY);
    if (fd < 0) { perror(path); return; }
    unsigned char buf[16];
    long long off = 0;
    int n;
    while ((n=read(fd, buf, sizeof(buf))) > 0) {
        printf("%08llx  ", off);
        for (int i=0; i<16; i++) {
            if (i<n) printf("%02x ", buf[i]);
            else printf("   ");
            if (i==7) printf(" ");
        }
        if (flag_C) {
            printf(" |");
            for (int i=0; i<n; i++) printf("%c", isprint(buf[i])?buf[i]:'.');
            printf("|");
        }
        printf("\n");
        off += n;
    }
    printf("%08llx\n", off);
    if (fd != STDIN_FILENO) close(fd);
}

int main(int argc, char **argv) {
    int i=1;
    if (i<argc && strcmp(argv[i],"-C")==0) { flag_C=1; i++; }
    if (i>=argc) dump("-");
    else for (; i<argc; i++) dump(argv[i]);
    return 0;
}
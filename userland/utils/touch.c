#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "touch: missing operand\n"); return 1; }
    int ret = 0;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_CREAT|O_WRONLY, 0644);
        if (fd < 0) { perror(argv[i]); ret = 1; continue; }
        close(fd);
    }
    return ret;
}
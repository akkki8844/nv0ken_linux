#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "umount: missing operand\n"); return 1; }
    int ret = 0;
    for (int i = 1; i < argc; i++) {
        if (umount(argv[i]) < 0) { perror(argv[i]); ret = 1; }
    }
    return ret;
}
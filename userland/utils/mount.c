#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc == 1) {
        FILE *f = fopen("/proc/mounts", "r");
        if (!f) f = fopen("/etc/mtab", "r");
        if (!f) { fprintf(stderr, "mount: cannot read mount table\n"); return 1; }
        char line[512];
        while (fgets(line, sizeof(line), f)) fputs(line, stdout);
        fclose(f);
        return 0;
    }
    if (argc < 4) { fprintf(stderr, "usage: mount device target fstype\n"); return 1; }
    if (mount(argv[1], argv[2], argv[3], 0) < 0) { perror("mount"); return 1; }
    return 0;
}
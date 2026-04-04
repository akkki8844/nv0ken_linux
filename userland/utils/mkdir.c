#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static int flag_p = 0;
static int flag_v = 0;

static int mkdir_p(const char *path, mode_t mode) {
    char buf[1024];
    strncpy(buf, path, sizeof(buf) - 1);
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(buf, mode) < 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    return mkdir(buf, mode);
}

int main(int argc, char **argv) {
    mode_t mode = 0755;
    int i;
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        for (char *f = argv[i]+1; *f; f++) {
            switch (*f) {
                case 'p': flag_p = 1; break;
                case 'v': flag_v = 1; break;
                case 'm': break;
            }
        }
    }
    if (i >= argc) { fprintf(stderr, "mkdir: missing operand\n"); return 1; }
    int ret = 0;
    for (; i < argc; i++) {
        int r = flag_p ? mkdir_p(argv[i], mode) : mkdir(argv[i], mode);
        if (r < 0 && !(flag_p && errno == EEXIST)) {
            perror(argv[i]); ret = 1;
        } else if (flag_v) {
            printf("mkdir: created directory '%s'\n", argv[i]);
        }
    }
    return ret;
}
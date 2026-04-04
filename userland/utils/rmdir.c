#include <stdio.h>
#include <unistd.h>
#include <string.h>

static int flag_p = 0;

int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        for (char *f = argv[i]+1; *f; f++)
            if (*f == 'p') flag_p = 1;
    }
    if (i >= argc) { fprintf(stderr, "rmdir: missing operand\n"); return 1; }
    int ret = 0;
    for (; i < argc; i++) {
        if (rmdir(argv[i]) < 0) { perror(argv[i]); ret = 1; continue; }
        if (flag_p) {
            char buf[1024];
            strncpy(buf, argv[i], sizeof(buf)-1);
            char *slash;
            while ((slash = strrchr(buf, '/')) && slash != buf) {
                *slash = '\0';
                if (rmdir(buf) < 0) break;
            }
        }
    }
    return ret;
}
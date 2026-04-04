#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static int flag_n = 0;
static int flag_e = 0;
static int flag_t = 0;

static int do_cat(const char *path) {
    int fd = strcmp(path, "-") == 0 ? STDIN_FILENO : open(path, O_RDONLY);
    if (fd < 0) { perror(path); return 1; }

    char buf[4096];
    int  line_num = 1;
    int  at_start = 1;
    int  n;

    if (!flag_n && !flag_e && !flag_t) {
        while ((n = read(fd, buf, sizeof(buf))) > 0)
            write(STDOUT_FILENO, buf, n);
    } else {
        char out[8192];
        int op = 0;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            for (int i = 0; i < n; i++) {
                unsigned char c = buf[i];
                if (flag_n && at_start) {
                    op += snprintf(out + op, sizeof(out) - op, "%6d\t", line_num++);
                    at_start = 0;
                }
                if (c == '\n') {
                    if (flag_e) out[op++] = '$';
                    out[op++] = '\n';
                    at_start = 1;
                } else if (c == '\t' && flag_t) {
                    out[op++] = '^'; out[op++] = 'I';
                } else {
                    out[op++] = c;
                }
                if (op > 7000) { write(STDOUT_FILENO, out, op); op = 0; }
            }
        }
        if (op > 0) write(STDOUT_FILENO, out, op);
    }

    if (fd != STDIN_FILENO) close(fd);
    return 0;
}

int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (char *f = argv[i]+1; *f; f++) {
            switch (*f) {
                case 'n': flag_n = 1; break;
                case 'e': flag_e = 1; break;
                case 't': flag_t = 1; break;
                case 'A': flag_e = flag_t = 1; break;
            }
        }
    }
    if (i >= argc) return do_cat("-");
    int ret = 0;
    for (; i < argc; i++) ret |= do_cat(argv[i]);
    return ret;
}
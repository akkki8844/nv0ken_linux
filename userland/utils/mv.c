#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define BUFSZ 65536

static int flag_v = 0;
static int flag_f = 0;

static int move_file(const char *src, const char *dst) {
    if (rename(src, dst) == 0) {
        if (flag_v) printf("'%s' -> '%s'\n", src, dst);
        return 0;
    }
    if (errno != EXDEV) { perror(dst); return 1; }
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) { perror(src); return 1; }
    struct stat st; fstat(sfd, &st);
    int dfd = open(dst, O_WRONLY|O_CREAT|O_TRUNC, st.st_mode & 0777);
    if (dfd < 0) { perror(dst); close(sfd); return 1; }
    char *buf = malloc(BUFSZ);
    int n, ret = 0;
    while ((n = read(sfd, buf, BUFSZ)) > 0)
        if (write(dfd, buf, n) != n) { ret = 1; break; }
    free(buf); close(sfd); close(dfd);
    if (!ret) unlink(src);
    if (!ret && flag_v) printf("'%s' -> '%s'\n", src, dst);
    return ret;
}

int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (char *f = argv[i]+1; *f; f++) {
            switch (*f) {
                case 'v': flag_v = 1; break;
                case 'f': flag_f = 1; break;
            }
        }
    }
    if (argc - i < 2) { fprintf(stderr, "mv: missing operand\n"); return 1; }
    char *dst = argv[argc-1];
    int ret = 0;
    struct stat dst_st;
    int dst_is_dir = stat(dst, &dst_st) == 0 && S_ISDIR(dst_st.st_mode);
    for (; i < argc-1; i++) {
        char target[1024];
        if (dst_is_dir) {
            const char *base = strrchr(argv[i], '/');
            base = base ? base+1 : argv[i];
            snprintf(target, sizeof(target), "%s/%s", dst, base);
        } else {
            strncpy(target, dst, sizeof(target)-1);
        }
        if (!flag_f) {
            struct stat s;
            if (stat(target, &s) == 0) {
                fprintf(stderr, "mv: overwrite '%s'? ", target);
                char c = getchar();
                if (c != 'y' && c != 'Y') continue;
            }
        }
        ret |= move_file(argv[i], target);
    }
    return ret;
}
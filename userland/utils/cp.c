#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define BUFSZ 65536

static int flag_r = 0;
static int flag_v = 0;
static int flag_f = 0;
static int flag_p = 0;

static int copy_file(const char *src, const char *dst) {
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) { perror(src); return 1; }
    struct stat st;
    fstat(sfd, &st);
    int dfd = open(dst, O_WRONLY|O_CREAT|O_TRUNC, st.st_mode & 0777);
    if (dfd < 0) { perror(dst); close(sfd); return 1; }
    char *buf = malloc(BUFSZ);
    if (!buf) { close(sfd); close(dfd); return 1; }
    int n, ret = 0;
    while ((n = read(sfd, buf, BUFSZ)) > 0) {
        if (write(dfd, buf, n) != n) { perror(dst); ret = 1; break; }
    }
    free(buf); close(sfd); close(dfd);
    if (flag_v) printf("'%s' -> '%s'\n", src, dst);
    return ret;
}

static int do_cp(const char *src, const char *dst);

static int cp_recursive(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) < 0) { perror(src); return 1; }
    if (!S_ISDIR(st.st_mode)) return copy_file(src, dst);
    mkdir(dst, st.st_mode & 0777);
    DIR *d = opendir(src);
    if (!d) { perror(src); return 1; }
    struct dirent *de;
    int ret = 0;
    while ((de = readdir(d))) {
        if (!strcmp(de->d_name,".") || !strcmp(de->d_name,"..")) continue;
        char s[1024], t[1024];
        snprintf(s, sizeof(s), "%s/%s", src, de->d_name);
        snprintf(t, sizeof(t), "%s/%s", dst, de->d_name);
        ret |= do_cp(s, t);
    }
    closedir(d);
    return ret;
}

static int do_cp(const char *src, const char *dst) {
    struct stat st;
    if (lstat(src, &st) < 0) { perror(src); return 1; }
    if (S_ISDIR(st.st_mode)) {
        if (!flag_r) { fprintf(stderr, "cp: omitting directory '%s'\n", src); return 1; }
        return cp_recursive(src, dst);
    }
    return copy_file(src, dst);
}

int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (char *f = argv[i]+1; *f; f++) {
            switch (*f) {
                case 'r': case 'R': flag_r = 1; break;
                case 'v': flag_v = 1; break;
                case 'f': flag_f = 1; break;
                case 'p': flag_p = 1; break;
            }
        }
    }
    if (argc - i < 2) { fprintf(stderr, "cp: missing operand\n"); return 1; }
    char *dst = argv[argc - 1];
    int ret = 0;
    struct stat dst_st;
    int dst_is_dir = stat(dst, &dst_st) == 0 && S_ISDIR(dst_st.st_mode);
    for (; i < argc - 1; i++) {
        char target[1024];
        if (dst_is_dir) {
            const char *base = strrchr(argv[i], '/');
            base = base ? base + 1 : argv[i];
            snprintf(target, sizeof(target), "%s/%s", dst, base);
        } else {
            strncpy(target, dst, sizeof(target)-1);
        }
        ret |= do_cp(argv[i], target);
    }
    return ret;
}
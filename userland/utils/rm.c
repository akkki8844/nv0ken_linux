#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static int flag_r = 0;
static int flag_f = 0;
static int flag_v = 0;

static int rm_recursive(const char *path) {
    struct stat st;
    if (lstat(path, &st) < 0) { if (!flag_f) perror(path); return flag_f ? 0 : 1; }

    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) { perror(path); return 1; }
        struct dirent *de;
        int ret = 0;
        while ((de = readdir(d))) {
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            char child[1024];
            snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
            ret |= rm_recursive(child);
        }
        closedir(d);
        if (rmdir(path) < 0) { perror(path); return 1; }
        if (flag_v) printf("removed directory '%s'\n", path);
        return ret;
    }

    if (unlink(path) < 0) { if (!flag_f) perror(path); return flag_f ? 0 : 1; }
    if (flag_v) printf("removed '%s'\n", path);
    return 0;
}

int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (char *f = argv[i]+1; *f; f++) {
            switch (*f) {
                case 'r': case 'R': flag_r = 1; break;
                case 'f': flag_f = 1; break;
                case 'v': flag_v = 1; break;
            }
        }
    }
    if (i >= argc) { if (!flag_f) fprintf(stderr, "rm: missing operand\n"); return flag_f ? 0 : 1; }
    int ret = 0;
    for (; i < argc; i++) {
        struct stat st;
        if (lstat(argv[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            if (!flag_r) { fprintf(stderr, "rm: cannot remove '%s': Is a directory\n", argv[i]); ret = 1; continue; }
            ret |= rm_recursive(argv[i]);
        } else {
            ret |= rm_recursive(argv[i]);
        }
    }
    return ret;
}
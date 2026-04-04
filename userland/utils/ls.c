#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#define COL_RESET  "\033[0m"
#define COL_DIR    "\033[1;34m"
#define COL_EXEC   "\033[1;32m"
#define COL_LINK   "\033[1;36m"

static int flag_l = 0;
static int flag_a = 0;
static int flag_h = 0;
static int flag_r = 0;
static int flag_1 = 0;

static void fmt_size(char *buf, size_t sz) {
    if (!flag_h) { snprintf(buf, 16, "%lld", (long long)sz); return; }
    if      (sz >= 1024*1024*1024) snprintf(buf, 16, "%.1fG", sz/(1024.0*1024*1024));
    else if (sz >= 1024*1024)      snprintf(buf, 16, "%.1fM", sz/(1024.0*1024));
    else if (sz >= 1024)           snprintf(buf, 16, "%.1fK", sz/1024.0);
    else                           snprintf(buf, 16, "%lld",  (long long)sz);
}

static void fmt_mode(char *buf, mode_t m) {
    buf[0] = S_ISDIR(m) ? 'd' : S_ISLNK(m) ? 'l' : '-';
    buf[1] = (m & S_IRUSR) ? 'r' : '-';
    buf[2] = (m & S_IWUSR) ? 'w' : '-';
    buf[3] = (m & S_IXUSR) ? 'x' : '-';
    buf[4] = (m & S_IRGRP) ? 'r' : '-';
    buf[5] = (m & S_IWGRP) ? 'w' : '-';
    buf[6] = (m & S_IXGRP) ? 'x' : '-';
    buf[7] = (m & S_IROTH) ? 'r' : '-';
    buf[8] = (m & S_IWOTH) ? 'w' : '-';
    buf[9] = (m & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';
}

static void print_entry(const char *dir, const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);

    struct stat st;
    int r = lstat(path, &st);

    if (flag_l) {
        char mode[12], sz[16];
        if (r < 0) {
            printf("??????????  ? ?    ?    ?    %s\n", name);
            return;
        }
        fmt_mode(mode, st.st_mode);
        fmt_size(sz, st.st_size);

        const char *col = S_ISDIR(st.st_mode)  ? COL_DIR  :
                          S_ISLNK(st.st_mode)   ? COL_LINK :
                          (st.st_mode & S_IXUSR) ? COL_EXEC : "";

        printf("%s %3d %-8d %-8d %8s %lld %s%s%s",
               mode, (int)st.st_nlink, (int)st.st_uid, (int)st.st_gid,
               sz, (long long)st.st_mtime,
               col, name, *col ? COL_RESET : "");

        if (S_ISLNK(st.st_mode)) {
            char tgt[512] = {0};
            readlink(path, tgt, sizeof(tgt) - 1);
            printf(" -> %s", tgt);
        }
        printf("\n");
    } else {
        const char *col = "";
        if (r == 0) {
            col = S_ISDIR(st.st_mode)   ? COL_DIR  :
                  S_ISLNK(st.st_mode)   ? COL_LINK :
                  (st.st_mode & S_IXUSR) ? COL_EXEC : "";
        }
        printf("%s%s%s%s", col, name, *col ? COL_RESET : "",
               flag_1 ? "\n" : "  ");
    }
}

static int name_cmp(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static void do_ls(const char *path) {
    DIR *d = opendir(path);
    if (!d) { perror(path); return; }

    char *names[4096];
    int count = 0;
    struct dirent *de;

    while ((de = readdir(d)) && count < 4095) {
        if (!flag_a && de->d_name[0] == '.') continue;
        names[count++] = strdup(de->d_name);
    }
    closedir(d);

    qsort(names, count, sizeof(char *), name_cmp);

    if (flag_r) {
        for (int i = count/2 - 1, j = count - 1 - i; i < j; i++, j--) {
            char *tmp = names[i]; names[i] = names[j]; names[j] = tmp;
        }
    }

    for (int i = 0; i < count; i++) {
        print_entry(path, names[i]);
        free(names[i]);
    }
    if (!flag_l && !flag_1) printf("\n");
}

int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        for (char *f = argv[i]+1; *f; f++) {
            switch (*f) {
                case 'l': flag_l = 1; break;
                case 'a': flag_a = 1; break;
                case 'h': flag_h = 1; break;
                case 'r': flag_r = 1; break;
                case '1': flag_1 = 1; break;
                case 'A': flag_a = 1; break;
            }
        }
    }
    if (i >= argc) { do_ls("."); }
    else { for (; i < argc; i++) { if (argc - i > 1) printf("%s:\n", argv[i]); do_ls(argv[i]); } }
    return 0;
}
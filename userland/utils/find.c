#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static const char *name_pat = NULL;
static int type_d=0, type_f=0;
static int maxdepth = -1;

static int name_match(const char *name, const char *pat) {
    if (!pat) return 1;
    const char *p=pat, *s=name;
    while (*p && *s) {
        if (*p=='*') { p++; while (*s && !name_match(s,p)) s++; continue; }
        if (*p=='?' || *p==*s) { p++; s++; continue; }
        return 0;
    }
    while (*p=='*') p++;
    return !*p && !*s;
}

static void do_find(const char *path, int depth) {
    if (maxdepth >= 0 && depth > maxdepth) return;

    struct stat st;
    if (lstat(path, &st) < 0) return;

    const char *base = strrchr(path, '/');
    base = base ? base+1 : path;

    int type_ok = (!type_d && !type_f) ||
                  (type_d && S_ISDIR(st.st_mode)) ||
                  (type_f && S_ISREG(st.st_mode));
    int name_ok = name_match(base, name_pat);

    if (type_ok && name_ok) puts(path);

    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) return;
        struct dirent *de;
        while ((de=readdir(d))) {
            if (!strcmp(de->d_name,".") || !strcmp(de->d_name,"..")) continue;
            char child[1024];
            snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
            do_find(child, depth+1);
        }
        closedir(d);
    }
}

int main(int argc, char **argv) {
    const char *start = ".";
    int i = 1;
    if (i < argc && argv[i][0] != '-') { start = argv[i++]; }
    while (i < argc) {
        if (strcmp(argv[i],"-name")==0 && i+1<argc) { name_pat=argv[++i]; }
        else if (strcmp(argv[i],"-type")==0 && i+1<argc) {
            if (argv[++i][0]=='d') type_d=1;
            else if (argv[i][0]=='f') type_f=1;
        }
        else if (strcmp(argv[i],"-maxdepth")==0 && i+1<argc) maxdepth=atoi(argv[++i]);
        i++;
    }
    do_find(start, 0);
    return 0;
}
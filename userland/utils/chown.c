#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "chown: missing operand\n"); return 1; }
    uid_t uid = -1; gid_t gid = -1;
    char spec[256]; strncpy(spec, argv[1], sizeof(spec)-1);
    char *colon = strchr(spec, ':');
    if (colon) {
        *colon = '\0';
        if (*(colon+1)) gid = (gid_t)atoi(colon+1);
    }
    if (spec[0]) uid = (uid_t)atoi(spec);
    int ret = 0;
    for (int i = 2; i < argc; i++) {
        if (chown(argv[i], uid, gid) < 0) { perror(argv[i]); ret = 1; }
    }
    return ret;
}
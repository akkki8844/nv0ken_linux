#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "which: missing argument\n"); return 1; }
    const char *path = getenv("PATH");
    if (!path) path = "/bin:/usr/bin";
    int ret = 0;
    for (int i = 1; i < argc; i++) {
        if (strchr(argv[i], '/')) {
            if (access(argv[i], X_OK) == 0) { puts(argv[i]); continue; }
            fprintf(stderr, "which: no %s\n", argv[i]); ret = 1; continue;
        }
        char paths[4096]; strncpy(paths, path, sizeof(paths)-1);
        char *dir = strtok(paths, ":"); int found = 0;
        while (dir) {
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", dir, argv[i]);
            if (access(full, X_OK) == 0) { puts(full); found = 1; break; }
            dir = strtok(NULL, ":");
        }
        if (!found) { fprintf(stderr, "which: no %s in PATH\n", argv[i]); ret = 1; }
    }
    return ret;
}
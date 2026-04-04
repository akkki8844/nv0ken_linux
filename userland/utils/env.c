#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

int main(int argc, char **argv) {
    int i = 1;
    while (i < argc && strchr(argv[i], '=')) {
        putenv(argv[i]);
        i++;
    }
    if (i < argc) {
        char *args[256];
        int n = 0;
        for (; i < argc && n < 255; i++) args[n++] = argv[i];
        args[n] = NULL;
        execvp(args[0], args);
        perror(args[0]);
        return 127;
    }
    for (char **e = environ; *e; e++) puts(*e);
    return 0;
}
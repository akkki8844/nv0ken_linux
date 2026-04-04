#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

static int parse_signal(const char *s) {
    if (*s == '-') s++;
    struct { const char *name; int num; } sigs[] = {
        {"HUP",1},{"INT",2},{"QUIT",3},{"KILL",9},{"TERM",15},
        {"STOP",19},{"CONT",18},{"USR1",10},{"USR2",12},
        {"PIPE",13},{"ALRM",14},{"CHLD",17},{NULL,0}
    };
    for (int i = 0; sigs[i].name; i++)
        if (strcasecmp(s, sigs[i].name) == 0) return sigs[i].num;
    return atoi(s);
}

int main(int argc, char **argv) {
    int sig = SIGTERM;
    int i = 1;
    if (i < argc && argv[i][0] == '-') {
        sig = parse_signal(argv[i]);
        i++;
    }
    if (i >= argc) { fprintf(stderr, "kill: missing pid\n"); return 1; }
    int ret = 0;
    for (; i < argc; i++) {
        pid_t pid = (pid_t)atoi(argv[i]);
        if (kill(pid, sig) < 0) { perror(argv[i]); ret = 1; }
    }
    return ret;
}
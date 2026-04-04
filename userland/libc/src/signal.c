#include <signal.h>
#include <errno.h>
#include <string.h>

typedef void (*sighandler_t)(int);

static sighandler_t handlers[32] = {0};

sighandler_t signal(int sig, sighandler_t handler) {
    if (sig < 0 || sig >= 32) { errno = EINVAL; return SIG_ERR; }
    struct sigaction sa, old;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (sys_sigaction(sig, &sa, &old) < 0) return SIG_ERR;
    return old.sa_handler;
}

int raise(int sig) {
    return sys_kill(sys_getpid(), sig);
}
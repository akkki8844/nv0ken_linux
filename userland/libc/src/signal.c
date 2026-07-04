#include <signal.h>
#include <errno.h>
#include <string.h>

extern long sys_sigaction(int signal_number, const struct sigaction *action,
                          struct sigaction *previous);
extern long sys_kill(int process_id, int signal_number);
extern long sys_getpid(void);

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

int kill(int process_id, int sig) {
    long result = sys_kill(process_id, sig);
    if (result < 0) {
        errno = (int)-result;
        return -1;
    }
    return 0;
}

int sigaction(int sig, const struct sigaction *action, struct sigaction *old) {
    long result = sys_sigaction(sig, action, old);
    if (result < 0) {
        errno = (int)-result;
        return -1;
    }
    return 0;
}

int sigemptyset(sigset_t *set) {
    if (!set) { errno = EINVAL; return -1; }
    *set = 0;
    return 0;
}

int sigfillset(sigset_t *set) {
    if (!set) { errno = EINVAL; return -1; }
    *set = ~(sigset_t)0;
    return 0;
}

int sigaddset(sigset_t *set, int sig) {
    if (!set || sig <= 0 || sig > 64) { errno = EINVAL; return -1; }
    *set |= (sigset_t)1 << (sig - 1);
    return 0;
}

int sigdelset(sigset_t *set, int sig) {
    if (!set || sig <= 0 || sig > 64) { errno = EINVAL; return -1; }
    *set &= ~((sigset_t)1 << (sig - 1));
    return 0;
}

int sigismember(const sigset_t *set, int sig) {
    if (!set || sig <= 0 || sig > 64) { errno = EINVAL; return -1; }
    return (*set & ((sigset_t)1 << (sig - 1))) != 0;
}

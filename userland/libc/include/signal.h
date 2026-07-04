#ifndef NV0_SIGNAL_H
#define NV0_SIGNAL_H

#include <stdint.h>

typedef void (*sighandler_t)(int);
typedef uint64_t sigset_t;

struct sigaction {
    sighandler_t sa_handler;
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
};

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGABRT 6
#define SIGKILL 9
#define SIGSEGV 11
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19

sighandler_t signal(int signal_number, sighandler_t handler);
int raise(int signal_number);
int sigaction(int signal_number, const struct sigaction *action,
              struct sigaction *previous);
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signal_number);
int sigdelset(sigset_t *set, int signal_number);
int sigismember(const sigset_t *set, int signal_number);

#endif

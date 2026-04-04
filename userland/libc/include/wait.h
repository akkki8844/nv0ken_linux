#ifndef SYS_WAIT_H
#define SYS_WAIT_H

#include <sys/types.h>

#define WNOHANG   1
#define WUNTRACED 2
#define WCONTINUED 8

#define WIFEXITED(s)    (((s) & 0x7F) == 0)
#define WEXITSTATUS(s)  (((s) >> 8) & 0xFF)
#define WIFSIGNALED(s)  (((s) & 0x7F) != 0 && ((s) & 0x7F) != 0x7F)
#define WTERMSIG(s)     ((s) & 0x7F)
#define WIFSTOPPED(s)   (((s) & 0xFF) == 0x7F)
#define WSTOPSIG(s)     (((s) >> 8) & 0xFF)
#define WIFCONTINUED(s) ((s) == 0xFFFF)

pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);

#endif
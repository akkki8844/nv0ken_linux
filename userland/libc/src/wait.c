#include <errno.h>
#include <wait.h>

extern long sys_wait(int *status);
extern long sys_waitpid(pid_t process_id, int *status, int options);

pid_t wait(int *status)
{
    long result = sys_wait(status);
    if (result < 0) {
        errno = (int)-result;
        return -1;
    }
    return (pid_t)result;
}

pid_t waitpid(pid_t process_id, int *status, int options)
{
    long result = sys_waitpid(process_id, status, options);
    if (result < 0) {
        errno = (int)-result;
        return -1;
    }
    return (pid_t)result;
}

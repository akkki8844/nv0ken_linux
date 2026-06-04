#include <fcntl.h>

#include <errno.h>
#include <stdarg.h>

extern long sys_open(const char *path, int flags, int mode);
extern long sys_fcntl(int fd, int cmd, long arg);

static int ret_errno(long ret)
{
    if (ret < 0 && ret > -4096) {
        errno = (int)-ret;
        return -1;
    }
    return (int)ret;
}

int open(const char *path, int flags, ...)
{
    int mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }

    return ret_errno(sys_open(path, flags, mode));
}

int creat(const char *path, int mode)
{
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int fcntl(int fd, int cmd, ...)
{
    long arg = 0;
    va_list ap;
    va_start(ap, cmd);
    arg = va_arg(ap, long);
    va_end(ap);
    return ret_errno(sys_fcntl(fd, cmd, arg));
}

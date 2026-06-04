#include <sys/utsname.h>

#include <errno.h>

extern long sys_uname(struct utsname *buf);

int uname(struct utsname *buf)
{
    long ret = sys_uname(buf);
    if (ret < 0 && ret > -4096) {
        errno = (int)-ret;
        return -1;
    }
    return 0;
}

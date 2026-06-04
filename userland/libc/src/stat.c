#include <sys/stat.h>

#include <errno.h>

extern long sys_stat(const char *path, struct stat *buf);
extern long sys_fstat(int fd, struct stat *buf);
extern long sys_lstat(const char *path, struct stat *buf);
extern long sys_chmod(const char *path, mode_t mode);
extern long sys_chown(const char *path, uid_t owner, gid_t group);
extern long sys_mkdir(const char *path, mode_t mode);

static mode_t current_umask = 022;

static int ret_errno(long ret)
{
    if (ret < 0 && ret > -4096) {
        errno = (int)-ret;
        return -1;
    }
    return (int)ret;
}

int stat(const char *path, struct stat *buf)
{
    return ret_errno(sys_stat(path, buf));
}

int fstat(int fd, struct stat *buf)
{
    return ret_errno(sys_fstat(fd, buf));
}

int lstat(const char *path, struct stat *buf)
{
    return ret_errno(sys_lstat(path, buf));
}

int chmod(const char *path, mode_t mode)
{
    return ret_errno(sys_chmod(path, mode));
}

int fchmod(int fd, mode_t mode)
{
    (void)fd;
    (void)mode;
    errno = ENOSYS;
    return -1;
}

int chown(const char *path, uid_t owner, gid_t group)
{
    return ret_errno(sys_chown(path, owner, group));
}

int mkdir(const char *path, mode_t mode)
{
    return ret_errno(sys_mkdir(path, mode & ~current_umask));
}

mode_t umask(mode_t mask)
{
    mode_t old = current_umask;
    current_umask = mask;
    return old;
}

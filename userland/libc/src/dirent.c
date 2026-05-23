#include <dirent.h>

#include <errno.h>
#include <stdlib.h>

struct DIR {
    int fd;
    struct dirent entry;
};

extern long sys_opendir(const char *path);
extern long sys_readdir(int fd, struct dirent *entry);
extern long sys_closedir(int fd);

DIR *opendir(const char *path)
{
    long fd = sys_opendir(path);
    if (fd < 0) {
        errno = (int)-fd;
        return 0;
    }

    DIR *dir = calloc(1, sizeof(*dir));
    if (!dir) {
        sys_closedir((int)fd);
        errno = ENOMEM;
        return 0;
    }

    dir->fd = (int)fd;
    return dir;
}

struct dirent *readdir(DIR *dir)
{
    if (!dir || dir->fd < 0) {
        errno = EBADF;
        return 0;
    }

    long rc = sys_readdir(dir->fd, &dir->entry);
    if (rc <= 0) {
        if (rc < 0) {
            errno = (int)-rc;
        }
        return 0;
    }

    dir->entry.d_name[sizeof(dir->entry.d_name) - 1] = '\0';
    return &dir->entry;
}

int closedir(DIR *dir)
{
    if (!dir) {
        errno = EBADF;
        return -1;
    }

    long rc = sys_closedir(dir->fd);
    free(dir);
    if (rc < 0) {
        errno = (int)-rc;
        return -1;
    }
    return 0;
}

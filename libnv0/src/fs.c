#include "../include/nv0/fs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stat.h>

#define NV_D_NAME_MAX 256

struct nv_dirent {
    ino_t d_ino;
    unsigned char d_type;
    char d_name[NV_D_NAME_MAX];
};

struct NvDir {
    int fd;
    struct nv_dirent entry;
};

extern long sys_opendir(const char *path);
extern long sys_readdir(int fd, struct nv_dirent *entry);
extern long sys_closedir(int fd);

int nv_open(const char *path, int flags, mode_t mode)
{
    return open(path, flags, mode);
}

int nv_mkdir(const char *path, mode_t mode)
{
    return mkdir(path, mode);
}

NvDir *nv_opendir(const char *path)
{
    long fd = sys_opendir(path);
    if (fd < 0) {
        errno = (int)-fd;
        return 0;
    }

    NvDir *dir = calloc(1, sizeof(*dir));
    if (!dir) {
        sys_closedir((int)fd);
        errno = ENOMEM;
        return 0;
    }

    dir->fd = (int)fd;
    return dir;
}

const char *nv_readdir(NvDir *dir)
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

    dir->entry.d_name[NV_D_NAME_MAX - 1] = '\0';
    return dir->entry.d_name;
}

void nv_closedir(NvDir *dir)
{
    if (!dir) {
        return;
    }
    if (dir->fd >= 0) {
        sys_closedir(dir->fd);
    }
    free(dir);
}

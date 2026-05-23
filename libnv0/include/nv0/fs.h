#ifndef NV0_FS_H
#define NV0_FS_H

#include <stddef.h>
#include <sys/types.h>

#define NV_O_RDONLY   0x0000
#define NV_O_WRONLY   0x0001
#define NV_O_RDWR     0x0002
#define NV_O_CREAT    0x0040
#define NV_O_TRUNC    0x0200
#define NV_O_APPEND   0x0400

typedef struct NvDir NvDir;

int         nv_open(const char *path, int flags, mode_t mode);
int         nv_mkdir(const char *path, mode_t mode);
NvDir      *nv_opendir(const char *path);
const char *nv_readdir(NvDir *dir);
void        nv_closedir(NvDir *dir);

#endif

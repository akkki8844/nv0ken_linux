#ifndef NV0KEN_FS_FD_TABLE_H
#define NV0KEN_FS_FD_TABLE_H

#include <stddef.h>

#include "vfs_types.h"

#define FD_TABLE_MAX 128

typedef struct {
    vfs_node_t *node;
    size_t offset;
    int flags;
    int refs;
} fd_entry_t;

void fd_table_init(void);
int fd_alloc(vfs_node_t *node, int flags);
int fd_alloc_at(int fd, vfs_node_t *node, int flags);
int fd_close(int fd);
fd_entry_t *fd_get(int fd);
int fd_dup(int fd);
int fd_dup2(int oldfd, int newfd);
int fd_seek(int fd, long long offset, int whence);
int fd_read(int fd, void *buffer, size_t count);
int fd_write(int fd, const void *buffer, size_t count);

#endif

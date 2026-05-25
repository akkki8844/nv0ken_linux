#ifndef NV0KEN_FS_VFS_TYPES_H
#define NV0KEN_FS_VFS_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define VFS_NAME_MAX 64
#define VFS_PATH_MAX 256

typedef enum {
    VFS_NODE_FILE,
    VFS_NODE_DIR,
    VFS_NODE_DEVICE,
    VFS_NODE_PIPE
} vfs_node_type_t;

typedef struct vfs_node vfs_node_t;

typedef struct {
    int (*read)(vfs_node_t *node, size_t offset, void *buffer, size_t count);
    int (*write)(vfs_node_t *node, size_t offset, const void *buffer, size_t count);
    vfs_node_t *(*lookup)(vfs_node_t *dir, const char *name);
    int (*create)(vfs_node_t *dir, const char *name, vfs_node_type_t type, vfs_node_t **out);
    int (*truncate)(vfs_node_t *node, size_t size);
} vfs_ops_t;

struct vfs_node {
    char name[VFS_NAME_MAX];
    vfs_node_type_t type;
    uint32_t mode;
    uint64_t size;
    void *private_data;
    const vfs_ops_t *ops;
    vfs_node_t *parent;
    vfs_node_t *next;
    vfs_node_t *children;
};

#endif

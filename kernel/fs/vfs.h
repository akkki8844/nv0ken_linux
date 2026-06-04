#ifndef NV0KEN_FS_VFS_H
#define NV0KEN_FS_VFS_H

#include "vfs_types.h"

void vfs_init(vfs_node_t *root);
vfs_node_t *vfs_root(void);
int vfs_mount_root(vfs_node_t *root);
vfs_node_t *vfs_lookup(const char *path);
int vfs_create(const char *path, vfs_node_type_t type, vfs_node_t **out);
int vfs_unlink(const char *path);
int vfs_read(vfs_node_t *node, size_t offset, void *buffer, size_t count);
int vfs_write(vfs_node_t *node, size_t offset, const void *buffer, size_t count);
void vfs_attach_child(vfs_node_t *parent, vfs_node_t *child);

#endif

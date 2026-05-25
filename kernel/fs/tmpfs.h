#ifndef NV0KEN_FS_TMPFS_H
#define NV0KEN_FS_TMPFS_H

#include "vfs_types.h"

vfs_node_t *tmpfs_create_root(void);
int tmpfs_create_node(vfs_node_t *dir, const char *name, vfs_node_type_t type, vfs_node_t **out);

#endif

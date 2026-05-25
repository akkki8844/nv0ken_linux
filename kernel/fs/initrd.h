#ifndef NV0KEN_FS_INITRD_H
#define NV0KEN_FS_INITRD_H

#include <stddef.h>

#include "vfs_types.h"

int initrd_load(const void *image, size_t size, vfs_node_t *mountpoint);

#endif

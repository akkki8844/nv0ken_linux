#include "initrd.h"

#include "../lib/string.h"
#include "tmpfs.h"

typedef struct {
    char name[64];
    uint32_t offset;
    uint32_t size;
} initrd_entry_t;

int initrd_load(const void *image, size_t size, vfs_node_t *mountpoint)
{
    if (!image || size < sizeof(uint32_t) || !mountpoint) {
        return -1;
    }

    const unsigned char *bytes = image;
    uint32_t count = 0;
    memcpy(&count, bytes, sizeof(count));
    size_t table_size = sizeof(uint32_t) + (size_t)count * sizeof(initrd_entry_t);
    if (table_size > size) {
        return -1;
    }

    const initrd_entry_t *entries = (const void *)(bytes + sizeof(uint32_t));
    for (uint32_t index = 0; index < count; ++index) {
        if ((size_t)entries[index].offset + entries[index].size > size) {
            return -1;
        }

        vfs_node_t *node = 0;
        if (tmpfs_create_node(mountpoint, entries[index].name, VFS_NODE_FILE, &node) != 0) {
            return -1;
        }
        if (node->ops && node->ops->write) {
            node->ops->write(node, 0, bytes + entries[index].offset, entries[index].size);
        }
    }
    return 0;
}

#include "fd_table.h"

#include "../lib/string.h"
#include "vfs.h"

static fd_entry_t table[FD_TABLE_MAX];

void fd_table_init(void)
{
    memset(table, 0, sizeof(table));
}

int fd_alloc(vfs_node_t *node, int flags)
{
    if (!node) {
        return -1;
    }

    for (int fd = 0; fd < FD_TABLE_MAX; ++fd) {
        if (!table[fd].node) {
            table[fd].node = node;
            table[fd].offset = 0;
            table[fd].flags = flags;
            table[fd].refs = 1;
            return fd;
        }
    }
    return -1;
}

int fd_close(int fd)
{
    if (fd < 0 || fd >= FD_TABLE_MAX || !table[fd].node) {
        return -1;
    }

    memset(&table[fd], 0, sizeof(table[fd]));
    return 0;
}

fd_entry_t *fd_get(int fd)
{
    if (fd < 0 || fd >= FD_TABLE_MAX || !table[fd].node) {
        return 0;
    }
    return &table[fd];
}

int fd_read(int fd, void *buffer, size_t count)
{
    fd_entry_t *entry = fd_get(fd);
    if (!entry) {
        return -1;
    }

    int read = vfs_read(entry->node, entry->offset, buffer, count);
    if (read > 0) {
        entry->offset += (size_t)read;
    }
    return read;
}

int fd_write(int fd, const void *buffer, size_t count)
{
    fd_entry_t *entry = fd_get(fd);
    if (!entry) {
        return -1;
    }

    int written = vfs_write(entry->node, entry->offset, buffer, count);
    if (written > 0) {
        entry->offset += (size_t)written;
    }
    return written;
}

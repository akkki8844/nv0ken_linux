#include "pipe_fs.h"

#include "../ipc/pipe.h"
#include "../lib/string.h"
#include "../mm/heap.h"

static int pipefs_read(vfs_node_t *node, size_t offset, void *buffer, size_t count)
{
    (void)offset;
    return (int)pipe_read(node ? node->private_data : 0, buffer, count);
}

static int pipefs_write(vfs_node_t *node, size_t offset, const void *buffer, size_t count)
{
    (void)offset;
    return (int)pipe_write(node ? node->private_data : 0, buffer, count);
}

static const vfs_ops_t pipefs_ops = {
    .read = pipefs_read,
    .write = pipefs_write,
};

vfs_node_t *pipefs_create_node(const char *name)
{
    vfs_node_t *node = kcalloc(1, sizeof(vfs_node_t));
    ipc_pipe_t *pipe = kcalloc(1, sizeof(ipc_pipe_t));
    if (!node || !pipe || pipe_init(pipe, 0) != 0) {
        kfree(node);
        kfree(pipe);
        return 0;
    }

    const char *node_name = name ? name : "pipe";
    size_t length = strlen(node_name);
    if (length >= VFS_NAME_MAX) {
        length = VFS_NAME_MAX - 1;
    }
    memcpy(node->name, node_name, length);
    node->name[length] = '\0';
    node->type = VFS_NODE_PIPE;
    node->private_data = pipe;
    node->ops = &pipefs_ops;
    return node;
}

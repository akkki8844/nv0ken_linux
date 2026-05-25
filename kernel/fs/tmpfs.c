#include "tmpfs.h"

#include "../lib/string.h"
#include "../mm/heap.h"
#include "vfs.h"

typedef struct {
    unsigned char *data;
    size_t capacity;
} tmpfs_file_t;

static int tmpfs_read(vfs_node_t *node, size_t offset, void *buffer, size_t count)
{
    if (!node || !buffer || node->type != VFS_NODE_FILE) {
        return -1;
    }

    tmpfs_file_t *file = node->private_data;
    if (!file || offset >= node->size) {
        return 0;
    }
    if (offset + count > node->size) {
        count = (size_t)node->size - offset;
    }
    memcpy(buffer, file->data + offset, count);
    return (int)count;
}

static int tmpfs_write(vfs_node_t *node, size_t offset, const void *buffer, size_t count)
{
    if (!node || !buffer || node->type != VFS_NODE_FILE) {
        return -1;
    }

    tmpfs_file_t *file = node->private_data;
    if (!file) {
        file = kcalloc(1, sizeof(*file));
        if (!file) {
            return -1;
        }
        node->private_data = file;
    }

    size_t required = offset + count;
    if (required > file->capacity) {
        size_t capacity = file->capacity ? file->capacity : 128;
        while (capacity < required) {
            capacity *= 2;
        }
        unsigned char *data = krealloc(file->data, capacity);
        if (!data) {
            return -1;
        }
        if (capacity > file->capacity) {
            memset(data + file->capacity, 0, capacity - file->capacity);
        }
        file->data = data;
        file->capacity = capacity;
    }

    memcpy(file->data + offset, buffer, count);
    if (required > node->size) {
        node->size = required;
    }
    return (int)count;
}

static vfs_node_t *tmpfs_lookup(vfs_node_t *dir, const char *name)
{
    if (!dir || !name || dir->type != VFS_NODE_DIR) {
        return 0;
    }

    for (vfs_node_t *child = dir->children; child; child = child->next) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
    }
    return 0;
}

static int tmpfs_truncate(vfs_node_t *node, size_t size)
{
    if (!node || node->type != VFS_NODE_FILE) {
        return -1;
    }

    tmpfs_file_t *file = node->private_data;
    if (!file) {
        file = kcalloc(1, sizeof(*file));
        if (!file) {
            return -1;
        }
        node->private_data = file;
    }

    if (size > file->capacity) {
        unsigned char *data = krealloc(file->data, size);
        if (!data) {
            return -1;
        }
        memset(data + file->capacity, 0, size - file->capacity);
        file->data = data;
        file->capacity = size;
    }
    node->size = size;
    return 0;
}

static const vfs_ops_t tmpfs_ops = {
    .read = tmpfs_read,
    .write = tmpfs_write,
    .lookup = tmpfs_lookup,
    .create = tmpfs_create_node,
    .truncate = tmpfs_truncate,
};

int tmpfs_create_node(vfs_node_t *dir, const char *name, vfs_node_type_t type, vfs_node_t **out)
{
    if (!dir || !name || dir->type != VFS_NODE_DIR || tmpfs_lookup(dir, name)) {
        return -1;
    }

    vfs_node_t *node = kcalloc(1, sizeof(vfs_node_t));
    if (!node) {
        return -1;
    }

    size_t length = strlen(name);
    if (length >= VFS_NAME_MAX) {
        length = VFS_NAME_MAX - 1;
    }
    memcpy(node->name, name, length);
    node->name[length] = '\0';
    node->type = type;
    node->ops = &tmpfs_ops;
    vfs_attach_child(dir, node);
    if (out) {
        *out = node;
    }
    return 0;
}

vfs_node_t *tmpfs_create_root(void)
{
    vfs_node_t *root = kcalloc(1, sizeof(vfs_node_t));
    if (!root) {
        return 0;
    }

    root->name[0] = '/';
    root->type = VFS_NODE_DIR;
    root->ops = &tmpfs_ops;
    return root;
}

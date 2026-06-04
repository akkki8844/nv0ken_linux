#include "vfs.h"

#include "../lib/string.h"
#include "path.h"

static vfs_node_t *root_node;

void vfs_init(vfs_node_t *root)
{
    root_node = root;
}

vfs_node_t *vfs_root(void)
{
    return root_node;
}

int vfs_mount_root(vfs_node_t *root)
{
    if (!root || root->type != VFS_NODE_DIR) {
        return -1;
    }

    root_node = root;
    root_node->parent = root_node;
    return 0;
}

void vfs_attach_child(vfs_node_t *parent, vfs_node_t *child)
{
    if (!parent || !child) {
        return;
    }

    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}

vfs_node_t *vfs_lookup(const char *path)
{
    if (!root_node || !path) {
        return 0;
    }

    char normalized[VFS_PATH_MAX];
    if (path_normalize(path, normalized, sizeof(normalized)) != 0) {
        return 0;
    }

    vfs_node_t *node = root_node;
    const char *cursor = normalized;
    char component[VFS_NAME_MAX];
    while (path_next_component(&cursor, component, sizeof(component)) > 0) {
        if (strcmp(component, ".") == 0) {
            continue;
        }
        if (strcmp(component, "..") == 0) {
            node = node->parent ? node->parent : node;
            continue;
        }
        if (!node->ops || !node->ops->lookup) {
            return 0;
        }
        node = node->ops->lookup(node, component);
        if (!node) {
            return 0;
        }
    }

    return node;
}

int vfs_create(const char *path, vfs_node_type_t type, vfs_node_t **out)
{
    if (!root_node || !path) {
        return -1;
    }

    char normalized[VFS_PATH_MAX];
    if (path_normalize(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }

    char *slash = 0;
    for (char *cursor = normalized; *cursor; ++cursor) {
        if (*cursor == '/') {
            slash = cursor;
        }
    }

    const char *name = slash ? slash + 1 : normalized;
    if (!name[0]) {
        return -1;
    }

    vfs_node_t *parent = root_node;
    if (slash && slash != normalized) {
        *slash = '\0';
        parent = vfs_lookup(normalized);
    }
    if (!parent || !parent->ops || !parent->ops->create) {
        return -1;
    }

    return parent->ops->create(parent, name, type, out);
}

int vfs_unlink(const char *path)
{
    vfs_node_t *node = vfs_lookup(path);
    if (!node || !node->parent || node == root_node) {
        return -1;
    }

    vfs_node_t **cursor = &node->parent->children;
    while (*cursor) {
        if (*cursor == node) {
            *cursor = node->next;
            node->parent = 0;
            node->next = 0;
            return 0;
        }
        cursor = &(*cursor)->next;
    }
    return -1;
}

int vfs_read(vfs_node_t *node, size_t offset, void *buffer, size_t count)
{
    if (!node || !node->ops || !node->ops->read) {
        return -1;
    }
    return node->ops->read(node, offset, buffer, count);
}

int vfs_write(vfs_node_t *node, size_t offset, const void *buffer, size_t count)
{
    if (!node || !node->ops || !node->ops->write) {
        return -1;
    }
    return node->ops->write(node, offset, buffer, count);
}

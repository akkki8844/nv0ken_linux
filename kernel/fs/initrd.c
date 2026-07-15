#include "initrd.h"

#include <stdbool.h>

#include "../lib/string.h"
#include "vfs.h"

#define TAR_BLOCK_SIZE 512u

typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char linkname[100];
    char magic[6];
    char version[2];
    char owner[32];
    char group[32];
    char major[8];
    char minor[8];
    char prefix[155];
    char padding[12];
} tar_header_t;

static bool is_zero_block(const unsigned char *block)
{
    for (size_t index = 0; index < TAR_BLOCK_SIZE; ++index) {
        if (block[index] != 0) {
            return false;
        }
    }
    return true;
}

static int parse_octal(const char *text, size_t length, size_t *value)
{
    size_t parsed = 0;
    bool seen_digit = false;
    for (size_t index = 0; index < length; ++index) {
        char ch = text[index];
        if (ch == '\0' || ch == ' ') {
            continue;
        }
        if (ch < '0' || ch > '7' || parsed > ((size_t)-1 >> 3)) {
            return -1;
        }
        parsed = (parsed << 3) | (size_t)(ch - '0');
        seen_digit = true;
    }
    *value = seen_digit ? parsed : 0;
    return 0;
}

static size_t copy_field(char *output, size_t output_size, const char *field, size_t field_size)
{
    size_t length = 0;
    while (length < field_size && field[length] != '\0') {
        ++length;
    }
    if (length >= output_size) {
        return 0;
    }
    memcpy(output, field, length);
    output[length] = '\0';
    return length;
}

static int tar_path(const tar_header_t *header, char *output, size_t output_size)
{
    char name[101];
    char prefix[156];
    size_t name_length = copy_field(name, sizeof(name), header->name, sizeof(header->name));
    size_t prefix_length = copy_field(prefix, sizeof(prefix), header->prefix, sizeof(header->prefix));
    if (name_length == 0) {
        return -1;
    }

    size_t required = 1 + prefix_length + (prefix_length ? 1 : 0) + name_length + 1;
    if (required > output_size) {
        return -1;
    }
    size_t offset = 0;
    output[offset++] = '/';
    if (prefix_length) {
        memcpy(output + offset, prefix, prefix_length);
        offset += prefix_length;
        output[offset++] = '/';
    }
    memcpy(output + offset, name, name_length);
    offset += name_length;
    output[offset] = '\0';
    return 0;
}

static int ensure_directories(const char *path)
{
    char directory[VFS_PATH_MAX];
    size_t length = strlen(path);
    if (length >= sizeof(directory)) {
        return -1;
    }
    memcpy(directory, path, length + 1);

    for (size_t index = 1; directory[index]; ++index) {
        if (directory[index] != '/') {
            continue;
        }
        directory[index] = '\0';
        if (!vfs_lookup(directory) && vfs_create(directory, VFS_NODE_DIR, 0) != 0) {
            return -1;
        }
        directory[index] = '/';
    }
    return 0;
}

int initrd_load(const void *image, size_t size, vfs_node_t *mountpoint)
{
    if (!image || !mountpoint || mountpoint != vfs_root() || size < TAR_BLOCK_SIZE) {
        return -1;
    }

    const unsigned char *bytes = image;
    size_t offset = 0;
    int loaded = 0;
    while (offset + TAR_BLOCK_SIZE <= size) {
        const tar_header_t *header = (const void *)(bytes + offset);
        if (is_zero_block(bytes + offset)) {
            return loaded;
        }

        size_t file_size;
        char path[VFS_PATH_MAX];
        if (parse_octal(header->size, sizeof(header->size), &file_size) != 0 ||
            tar_path(header, path, sizeof(path)) != 0) {
            return -1;
        }
        size_t data_offset = offset + TAR_BLOCK_SIZE;
        size_t padded_size = (file_size + TAR_BLOCK_SIZE - 1) & ~(TAR_BLOCK_SIZE - 1);
        if (file_size > size - data_offset || padded_size > size - data_offset) {
            return -1;
        }

        size_t path_length = strlen(path);
        while (path_length > 1 && path[path_length - 1] == '/') {
            path[--path_length] = '\0';
        }
        if (ensure_directories(path) != 0) {
            return -1;
        }

        if (header->type == '5') {
            if (!vfs_lookup(path) && vfs_create(path, VFS_NODE_DIR, 0) != 0) {
                return -1;
            }
        } else if (header->type == '\0' || header->type == '0') {
            vfs_node_t *node = vfs_lookup(path);
            if (!node && vfs_create(path, VFS_NODE_FILE, &node) != 0) {
                return -1;
            }
            if (!node || vfs_write(node, 0, bytes + data_offset, file_size) != (int)file_size) {
                return -1;
            }
        }
        ++loaded;
        offset = data_offset + padded_size;
    }
    return -1;
}

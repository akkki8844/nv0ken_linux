#include "ex2.h"

#include "../lib/string.h"

int ext2_probe(const void *image, size_t size)
{
    if (!image || size < 1024 + sizeof(ext2_superblock_t)) {
        return -1;
    }

    ext2_superblock_t superblock;
    memcpy(&superblock, (const unsigned char *)image + 1024, sizeof(superblock));
    if (superblock.magic != EXT2_SUPER_MAGIC) {
        return -1;
    }
    if (superblock.inode_count == 0 || superblock.block_count == 0) {
        return -1;
    }
    return 0;
}

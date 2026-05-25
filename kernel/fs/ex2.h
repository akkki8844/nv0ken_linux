#ifndef NV0KEN_FS_EX2_COMPAT_H
#define NV0KEN_FS_EX2_COMPAT_H

#include <stddef.h>

#include "ext2_structs.h"

int ext2_probe(const void *image, size_t size);

#endif

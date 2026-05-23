#ifndef NV0KEN_LIB_ASSERT_H
#define NV0KEN_LIB_ASSERT_H

#include "panic.h"

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            panic("assertion failed: %s at %s:%d", #expr, __FILE__, __LINE__); \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) ASSERT((ptr) != 0)

#endif

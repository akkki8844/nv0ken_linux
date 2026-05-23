#ifndef NV0KEN_LIB_MATH_H
#define NV0KEN_LIB_MATH_H

#include <stdint.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(v, lo, hi) (MIN(MAX((v), (lo)), (hi)))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static inline uint64_t align_down_u64(uint64_t value, uint64_t alignment)
{
    return value & ~(alignment - 1);
}

static inline uint64_t align_up_u64(uint64_t value, uint64_t alignment)
{
    return align_down_u64(value + alignment - 1, alignment);
}

static inline int is_power_of_two_u64(uint64_t value)
{
    return value != 0 && (value & (value - 1)) == 0;
}

static inline uint64_t div_round_up_u64(uint64_t value, uint64_t divisor)
{
    return (value + divisor - 1) / divisor;
}

#endif

#ifndef NV0KEN_LIB_BITMAP_H
#define NV0KEN_LIB_BITMAP_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t *words;
    size_t bit_count;
} bitmap_t;

void bitmap_init(bitmap_t *bitmap, uint64_t *storage, size_t bit_count);
void bitmap_clear_all(bitmap_t *bitmap);
void bitmap_set_all(bitmap_t *bitmap);
void bitmap_set(bitmap_t *bitmap, size_t bit);
void bitmap_clear(bitmap_t *bitmap, size_t bit);
int  bitmap_test(const bitmap_t *bitmap, size_t bit);
long bitmap_find_clear(const bitmap_t *bitmap);
long bitmap_find_clear_range(const bitmap_t *bitmap, size_t count);
size_t bitmap_count_set(const bitmap_t *bitmap);
size_t bitmap_storage_words(size_t bit_count);

#endif

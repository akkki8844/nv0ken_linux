#include "bitmap.h"

#include "string.h"

#define BITS_PER_WORD 64u

size_t bitmap_storage_words(size_t bit_count)
{
    return (bit_count + BITS_PER_WORD - 1) / BITS_PER_WORD;
}

void bitmap_init(bitmap_t *bitmap, uint64_t *storage, size_t bit_count)
{
    if (!bitmap) {
        return;
    }
    bitmap->words = storage;
    bitmap->bit_count = bit_count;
}

void bitmap_clear_all(bitmap_t *bitmap)
{
    if (!bitmap || !bitmap->words) {
        return;
    }
    memset(bitmap->words, 0, bitmap_storage_words(bitmap->bit_count) * sizeof(uint64_t));
}

void bitmap_set_all(bitmap_t *bitmap)
{
    if (!bitmap || !bitmap->words) {
        return;
    }
    for (size_t i = 0; i < bitmap_storage_words(bitmap->bit_count); ++i) {
        bitmap->words[i] = UINT64_MAX;
    }
}

void bitmap_set(bitmap_t *bitmap, size_t bit)
{
    if (!bitmap || !bitmap->words || bit >= bitmap->bit_count) {
        return;
    }
    bitmap->words[bit / BITS_PER_WORD] |= 1ull << (bit % BITS_PER_WORD);
}

void bitmap_clear(bitmap_t *bitmap, size_t bit)
{
    if (!bitmap || !bitmap->words || bit >= bitmap->bit_count) {
        return;
    }
    bitmap->words[bit / BITS_PER_WORD] &= ~(1ull << (bit % BITS_PER_WORD));
}

int bitmap_test(const bitmap_t *bitmap, size_t bit)
{
    if (!bitmap || !bitmap->words || bit >= bitmap->bit_count) {
        return 0;
    }
    return (bitmap->words[bit / BITS_PER_WORD] & (1ull << (bit % BITS_PER_WORD))) != 0;
}

long bitmap_find_clear(const bitmap_t *bitmap)
{
    if (!bitmap || !bitmap->words) {
        return -1;
    }
    for (size_t bit = 0; bit < bitmap->bit_count; ++bit) {
        if (!bitmap_test(bitmap, bit)) {
            return (long)bit;
        }
    }
    return -1;
}

long bitmap_find_clear_range(const bitmap_t *bitmap, size_t count)
{
    if (!bitmap || !bitmap->words || count == 0 || count > bitmap->bit_count) {
        return -1;
    }

    size_t run_start = 0;
    size_t run_length = 0;
    for (size_t bit = 0; bit < bitmap->bit_count; ++bit) {
        if (!bitmap_test(bitmap, bit)) {
            if (run_length == 0) {
                run_start = bit;
            }
            if (++run_length == count) {
                return (long)run_start;
            }
        } else {
            run_length = 0;
        }
    }
    return -1;
}

size_t bitmap_count_set(const bitmap_t *bitmap)
{
    if (!bitmap || !bitmap->words) {
        return 0;
    }

    size_t count = 0;
    for (size_t bit = 0; bit < bitmap->bit_count; ++bit) {
        count += bitmap_test(bitmap, bit) ? 1u : 0u;
    }
    return count;
}

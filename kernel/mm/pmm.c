#include "pmm.h"

#include "lib/bitmap.h"
#include "lib/math.h"
#include "lib/string.h"

#define PMM_MAX_FRAMES (1024u * 1024u)

static uint64_t frame_storage[PMM_MAX_FRAMES / 64];
static bitmap_t frame_map;
static size_t total_frames;
static size_t free_frames;

static size_t frame_index(uint64_t addr)
{
    return (size_t)(addr / PMM_FRAME_SIZE);
}

void pmm_init(uint64_t memory_top)
{
    total_frames = (size_t)MIN(memory_top / PMM_FRAME_SIZE, PMM_MAX_FRAMES);
    bitmap_init(&frame_map, frame_storage, total_frames);
    bitmap_set_all(&frame_map);
    free_frames = 0;
}

void pmm_mark_usable(uint64_t base, uint64_t length)
{
    uint64_t start = align_up_u64(base, PMM_FRAME_SIZE);
    uint64_t end = align_down_u64(base + length, PMM_FRAME_SIZE);
    for (uint64_t addr = start; addr < end; addr += PMM_FRAME_SIZE) {
        size_t index = frame_index(addr);
        if (index < total_frames && bitmap_test(&frame_map, index)) {
            bitmap_clear(&frame_map, index);
            ++free_frames;
        }
    }
}

void pmm_mark_reserved(uint64_t base, uint64_t length)
{
    uint64_t start = align_down_u64(base, PMM_FRAME_SIZE);
    uint64_t end = align_up_u64(base + length, PMM_FRAME_SIZE);
    for (uint64_t addr = start; addr < end; addr += PMM_FRAME_SIZE) {
        size_t index = frame_index(addr);
        if (index < total_frames && !bitmap_test(&frame_map, index)) {
            bitmap_set(&frame_map, index);
            --free_frames;
        }
    }
}

uint64_t pmm_alloc_frame(void)
{
    long index = bitmap_find_clear(&frame_map);
    if (index < 0) {
        return 0;
    }
    bitmap_set(&frame_map, (size_t)index);
    --free_frames;
    return (uint64_t)index * PMM_FRAME_SIZE;
}

uint64_t pmm_alloc_frames(size_t count)
{
    long index = bitmap_find_clear_range(&frame_map, count);
    if (index < 0) {
        return 0;
    }
    for (size_t i = 0; i < count; ++i) {
        bitmap_set(&frame_map, (size_t)index + i);
    }
    free_frames -= count;
    return (uint64_t)index * PMM_FRAME_SIZE;
}

void pmm_free_frame(uint64_t frame)
{
    pmm_free_frames(frame, 1);
}

void pmm_free_frames(uint64_t frame, size_t count)
{
    size_t start = frame_index(frame);
    for (size_t i = 0; i < count; ++i) {
        size_t index = start + i;
        if (index < total_frames && bitmap_test(&frame_map, index)) {
            bitmap_clear(&frame_map, index);
            ++free_frames;
        }
    }
}

size_t pmm_total_frames(void)
{
    return total_frames;
}

size_t pmm_free_count(void)
{
    return free_frames;
}

size_t pmm_used_count(void)
{
    return total_frames - free_frames;
}

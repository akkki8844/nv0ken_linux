#ifndef NV0KEN_MM_PMM_H
#define NV0KEN_MM_PMM_H

#include <stddef.h>
#include <stdint.h>

#define PMM_FRAME_SIZE 4096ull

void pmm_init(uint64_t memory_top);
void pmm_mark_usable(uint64_t base, uint64_t length);
void pmm_mark_reserved(uint64_t base, uint64_t length);
uint64_t pmm_alloc_frame(void);
uint64_t pmm_alloc_frames(size_t count);
void pmm_free_frame(uint64_t frame);
void pmm_free_frames(uint64_t frame, size_t count);
size_t pmm_total_frames(void);
size_t pmm_free_count(void);
size_t pmm_used_count(void);

#endif

#ifndef NV0KEN_MM_MMAP_H
#define NV0KEN_MM_MMAP_H

#include <limine.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MMAP_MAX_REGIONS 128

typedef enum {
    MMAP_REGION_USABLE,
    MMAP_REGION_RESERVED,
    MMAP_REGION_ACPI,
    MMAP_REGION_BOOTLOADER,
    MMAP_REGION_KERNEL,
    MMAP_REGION_FRAMEBUFFER,
    MMAP_REGION_BAD,
} mmap_region_type_t;

typedef struct {
    uint64_t base;
    uint64_t length;
    mmap_region_type_t type;
} mmap_region_t;

void mmap_init_from_limine(struct limine_memmap_response *response);
void mmap_reset(void);
bool mmap_add_region(uint64_t base, uint64_t length, mmap_region_type_t type);
size_t mmap_region_count(void);
const mmap_region_t *mmap_region_at(size_t index);
uint64_t mmap_total_usable(void);
uint64_t mmap_highest_addr(void);
mmap_region_type_t mmap_type_from_limine(uint64_t limine_type);

#endif

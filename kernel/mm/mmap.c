#include "mmap.h"

#include "lib/string.h"

static mmap_region_t regions[MMAP_MAX_REGIONS];
static size_t region_count;

mmap_region_type_t mmap_type_from_limine(uint64_t limine_type)
{
    switch (limine_type) {
    case LIMINE_MEMMAP_USABLE:
        return MMAP_REGION_USABLE;
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
    case LIMINE_MEMMAP_ACPI_NVS:
        return MMAP_REGION_ACPI;
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        return MMAP_REGION_BOOTLOADER;
    case LIMINE_MEMMAP_KERNEL_AND_MODULES:
        return MMAP_REGION_KERNEL;
    case LIMINE_MEMMAP_FRAMEBUFFER:
        return MMAP_REGION_FRAMEBUFFER;
    case LIMINE_MEMMAP_BAD_MEMORY:
        return MMAP_REGION_BAD;
    default:
        return MMAP_REGION_RESERVED;
    }
}

void mmap_init_from_limine(struct limine_memmap_response *response)
{
    memset(regions, 0, sizeof(regions));
    region_count = 0;
    if (!response) {
        return;
    }

    for (uint64_t i = 0; i < response->entry_count && region_count < MMAP_MAX_REGIONS; ++i) {
        struct limine_memmap_entry *entry = response->entries[i];
        if (!entry || entry->length == 0) {
            continue;
        }
        regions[region_count].base = entry->base;
        regions[region_count].length = entry->length;
        regions[region_count].type = mmap_type_from_limine(entry->type);
        ++region_count;
    }
}

size_t mmap_region_count(void)
{
    return region_count;
}

const mmap_region_t *mmap_region_at(size_t index)
{
    return index < region_count ? &regions[index] : 0;
}

uint64_t mmap_total_usable(void)
{
    uint64_t total = 0;
    for (size_t i = 0; i < region_count; ++i) {
        if (regions[i].type == MMAP_REGION_USABLE ||
            regions[i].type == MMAP_REGION_BOOTLOADER) {
            total += regions[i].length;
        }
    }
    return total;
}

uint64_t mmap_highest_addr(void)
{
    uint64_t highest = 0;
    for (size_t i = 0; i < region_count; ++i) {
        uint64_t end = regions[i].base + regions[i].length;
        if (end > highest) {
            highest = end;
        }
    }
    return highest;
}

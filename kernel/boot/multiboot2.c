#include "multiboot2.h"

#include "mm/mmap.h"

enum {
    MULTIBOOT2_TAG_END = 0,
    MULTIBOOT2_TAG_MODULE = 3,
    MULTIBOOT2_TAG_MEMORY_MAP = 6,
    MULTIBOOT2_TAG_FRAMEBUFFER = 8,
    MULTIBOOT2_TAG_ACPI_OLD = 14,
    MULTIBOOT2_TAG_ACPI_NEW = 15,
};

typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) multiboot2_tag_t;

typedef struct {
    multiboot2_tag_t tag;
    uint32_t start;
    uint32_t end;
    char command_line[];
} __attribute__((packed)) multiboot2_module_tag_t;

typedef struct {
    uint64_t address;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed)) multiboot2_memory_entry_t;

typedef struct {
    multiboot2_tag_t tag;
    uint32_t entry_size;
    uint32_t entry_version;
    multiboot2_memory_entry_t entries[];
} __attribute__((packed)) multiboot2_memory_map_tag_t;

typedef struct {
    multiboot2_tag_t tag;
    uint64_t address;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t type;
    uint16_t reserved;
} __attribute__((packed)) multiboot2_framebuffer_tag_t;

uint64_t multiboot2_boot_magic;
uint64_t multiboot2_boot_info;

static const multiboot2_tag_t *find_tag(uint32_t wanted, size_t occurrence)
{
    if (!multiboot2_available()) {
        return 0;
    }

    const uint8_t *base = (const uint8_t *)(uintptr_t)multiboot2_boot_info;
    uint32_t total_size = *(const uint32_t *)base;
    if (total_size < 16) {
        return 0;
    }

    const uint8_t *cursor = base + 8;
    const uint8_t *end = base + total_size;
    while (cursor + sizeof(multiboot2_tag_t) <= end) {
        const multiboot2_tag_t *tag = (const multiboot2_tag_t *)cursor;
        if (tag->type == MULTIBOOT2_TAG_END || tag->size < sizeof(*tag)) {
            break;
        }
        if (tag->type == wanted && occurrence-- == 0) {
            return tag;
        }
        cursor += (tag->size + 7u) & ~7u;
    }
    return 0;
}

bool multiboot2_available(void)
{
    return multiboot2_boot_magic == MULTIBOOT2_BOOTLOADER_MAGIC &&
           multiboot2_boot_info != 0;
}

void multiboot2_init_memory_map(void)
{
    const multiboot2_memory_map_tag_t *map =
        (const multiboot2_memory_map_tag_t *)find_tag(MULTIBOOT2_TAG_MEMORY_MAP, 0);
    mmap_reset();
    if (!map || map->entry_size < sizeof(multiboot2_memory_entry_t)) {
        return;
    }

    const uint8_t *cursor = (const uint8_t *)map->entries;
    const uint8_t *end = (const uint8_t *)map + map->tag.size;
    while (cursor + map->entry_size <= end) {
        const multiboot2_memory_entry_t *entry =
            (const multiboot2_memory_entry_t *)cursor;
        mmap_region_type_t type = entry->type == 1
                                      ? MMAP_REGION_USABLE
                                      : MMAP_REGION_RESERVED;
        mmap_add_region(entry->address, entry->length, type);
        cursor += map->entry_size;
    }
}

bool multiboot2_module(size_t index, const void **address, size_t *size)
{
    const multiboot2_module_tag_t *module =
        (const multiboot2_module_tag_t *)find_tag(MULTIBOOT2_TAG_MODULE, index);
    if (!module || module->end < module->start) {
        return false;
    }
    if (address) {
        *address = (const void *)(uintptr_t)module->start;
    }
    if (size) {
        *size = module->end - module->start;
    }
    return true;
}

bool multiboot2_framebuffer(multiboot2_framebuffer_t *framebuffer)
{
    const multiboot2_framebuffer_tag_t *tag =
        (const multiboot2_framebuffer_tag_t *)find_tag(MULTIBOOT2_TAG_FRAMEBUFFER, 0);
    if (!tag || !framebuffer || tag->type != 1 || tag->bpp != 32) {
        return false;
    }
    framebuffer->address = tag->address;
    framebuffer->pitch = tag->pitch;
    framebuffer->width = tag->width;
    framebuffer->height = tag->height;
    framebuffer->bpp = tag->bpp;
    return true;
}

const void *multiboot2_rsdp(void)
{
    const multiboot2_tag_t *tag = find_tag(MULTIBOOT2_TAG_ACPI_NEW, 0);
    if (!tag) {
        tag = find_tag(MULTIBOOT2_TAG_ACPI_OLD, 0);
    }
    return tag ? (const uint8_t *)tag + sizeof(*tag) : 0;
}

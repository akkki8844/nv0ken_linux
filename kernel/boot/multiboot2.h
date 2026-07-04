#ifndef NV0KEN_BOOT_MULTIBOOT2_H
#define NV0KEN_BOOT_MULTIBOOT2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289u

typedef struct {
    uint64_t address;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
} multiboot2_framebuffer_t;

extern uint64_t multiboot2_boot_magic;
extern uint64_t multiboot2_boot_info;

bool multiboot2_available(void);
void multiboot2_init_memory_map(void);
bool multiboot2_module(size_t index, const void **address, size_t *size);
bool multiboot2_framebuffer(multiboot2_framebuffer_t *framebuffer);
const void *multiboot2_rsdp(void);

#endif

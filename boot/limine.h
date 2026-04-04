#ifndef LIMINE_H
#define LIMINE_H

#include <stdint.h>
#include <stddef.h>

#define LIMINE_COMMON_MAGIC  0xc7b1dd30df4c8b88, 0x0a82e883a194f07b

/* -----------------------------------------------------------------------
 * Request/response IDs
 * --------------------------------------------------------------------- */

#define LIMINE_FRAMEBUFFER_REQUEST_ID \
    LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b

#define LIMINE_MEMMAP_REQUEST_ID \
    LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62

#define LIMINE_HHDM_REQUEST_ID \
    LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b

#define LIMINE_KERNEL_ADDRESS_REQUEST_ID \
    LIMINE_COMMON_MAGIC, 0x71ba76863cc55f63, 0xb2644a48c516a487

#define LIMINE_RSDP_REQUEST_ID \
    LIMINE_COMMON_MAGIC, 0xc5e77b6b397e7b43, 0x27637845accdcf3c

#define LIMINE_SMP_REQUEST_ID \
    LIMINE_COMMON_MAGIC, 0x95a67b819a1b857e, 0xa0b61b723b6a73e0

#define LIMINE_MODULE_REQUEST_ID \
    LIMINE_COMMON_MAGIC, 0x3e7e279702be32af, 0xca1c4f3bd1280cee

#define LIMINE_BOOT_TIME_REQUEST_ID \
    LIMINE_COMMON_MAGIC, 0x502746e184c088aa, 0xfbc5ec83e6327893

#define LIMINE_KERNEL_FILE_REQUEST_ID \
    LIMINE_COMMON_MAGIC, 0xad97e90e83f1ed67, 0x31eb5d1c5ff23b69

#define LIMINE_STACK_SIZE_REQUEST_ID \
    LIMINE_COMMON_MAGIC, 0x224ef0460a8e8926, 0xe1cb0fc25f46ea3d

#define LIMINE_TERMINAL_REQUEST_ID \
    LIMINE_COMMON_MAGIC, 0xc8ac59310c2b0844, 0xa68d0c7265d38878

/* -----------------------------------------------------------------------
 * Base request/response structures
 * --------------------------------------------------------------------- */

struct limine_uuid {
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint8_t  d[8];
};

struct limine_file {
    uint64_t  revision;
    void     *address;
    uint64_t  size;
    char     *path;
    char     *cmdline;
    uint32_t  media_type;
    uint32_t  unused;
    uint32_t  tftp_ip;
    uint32_t  tftp_port;
    uint32_t  partition_index;
    uint32_t  mbr_disk_id;
    struct limine_uuid gpt_disk_uuid;
    struct limine_uuid gpt_part_uuid;
    struct limine_uuid part_uuid;
};

/* -----------------------------------------------------------------------
 * Framebuffer
 * --------------------------------------------------------------------- */

#define LIMINE_FRAMEBUFFER_RGB  0

struct limine_video_mode {
    uint64_t pitch;
    uint64_t width;
    uint64_t height;
    uint16_t bpp;
    uint8_t  memory_model;
    uint8_t  red_mask_size;
    uint8_t  red_mask_shift;
    uint8_t  green_mask_size;
    uint8_t  green_mask_shift;
    uint8_t  blue_mask_size;
    uint8_t  blue_mask_shift;
};

struct limine_framebuffer {
    void     *address;
    uint64_t  width;
    uint64_t  height;
    uint64_t  pitch;
    uint16_t  bpp;
    uint8_t   memory_model;
    uint8_t   red_mask_size;
    uint8_t   red_mask_shift;
    uint8_t   green_mask_size;
    uint8_t   green_mask_shift;
    uint8_t   blue_mask_size;
    uint8_t   blue_mask_shift;
    uint8_t   unused[7];
    uint64_t  edid_size;
    void     *edid;
    uint64_t  mode_count;
    struct limine_video_mode **modes;
};

struct limine_framebuffer_response {
    uint64_t  revision;
    uint64_t  framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_framebuffer_response *response;
};

/* -----------------------------------------------------------------------
 * Memory map
 * --------------------------------------------------------------------- */

#define LIMINE_MEMMAP_USABLE                 0
#define LIMINE_MEMMAP_RESERVED               1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE       2
#define LIMINE_MEMMAP_ACPI_NVS               3
#define LIMINE_MEMMAP_BAD_MEMORY             4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_KERNEL_AND_MODULES     6
#define LIMINE_MEMMAP_FRAMEBUFFER            7

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response {
    uint64_t  revision;
    uint64_t  entry_count;
    struct limine_memmap_entry **entries;
};

struct limine_memmap_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_memmap_response *response;
};

/* -----------------------------------------------------------------------
 * Higher half direct map (HHDM)
 * --------------------------------------------------------------------- */

struct limine_hhdm_response {
    uint64_t revision;
    uint64_t offset;
};

struct limine_hhdm_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_hhdm_response *response;
};

/* -----------------------------------------------------------------------
 * Kernel virtual/physical address
 * --------------------------------------------------------------------- */

struct limine_kernel_address_response {
    uint64_t revision;
    uint64_t physical_base;
    uint64_t virtual_base;
};

struct limine_kernel_address_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_kernel_address_response *response;
};

/* -----------------------------------------------------------------------
 * RSDP
 * --------------------------------------------------------------------- */

struct limine_rsdp_response {
    uint64_t  revision;
    void     *address;
};

struct limine_rsdp_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_rsdp_response *response;
};

/* -----------------------------------------------------------------------
 * SMP
 * --------------------------------------------------------------------- */

struct limine_smp_info {
    uint32_t  processor_id;
    uint32_t  lapic_id;
    uint64_t  reserved;
    void     (*goto_address)(struct limine_smp_info *);
    uint64_t  extra_argument;
};

struct limine_smp_response {
    uint64_t  revision;
    uint32_t  flags;
    uint32_t  bsp_lapic_id;
    uint64_t  cpu_count;
    struct limine_smp_info **cpus;
};

struct limine_smp_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_smp_response *response;
    uint64_t flags;
};

/* -----------------------------------------------------------------------
 * Modules
 * --------------------------------------------------------------------- */

struct limine_module_response {
    uint64_t  revision;
    uint64_t  module_count;
    struct limine_file **modules;
};

struct limine_module_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_module_response *response;
};

/* -----------------------------------------------------------------------
 * Boot time
 * --------------------------------------------------------------------- */

struct limine_boot_time_response {
    uint64_t revision;
    int64_t  boot_time;
};

struct limine_boot_time_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_boot_time_response *response;
};

/* -----------------------------------------------------------------------
 * Kernel file
 * --------------------------------------------------------------------- */

struct limine_kernel_file_response {
    uint64_t          revision;
    struct limine_file *kernel_file;
};

struct limine_kernel_file_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_kernel_file_response *response;
};

/* -----------------------------------------------------------------------
 * Stack size
 * --------------------------------------------------------------------- */

struct limine_stack_size_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_stack_size_response *response;
    uint64_t stack_size;
};

struct limine_stack_size_response {
    uint64_t revision;
};

/* -----------------------------------------------------------------------
 * Request placement macro
 * Used in kernel/main.c to declare requests in the correct ELF section
 * so Limine can find them before transferring control to the kernel.
 * --------------------------------------------------------------------- */

#define LIMINE_REQUEST \
    __attribute__((used, section(".limine_requests")))

#define LIMINE_REQUEST_TERMINATE \
    __attribute__((used, section(".limine_requests_end"))) \
    static volatile uint64_t _limine_requests_end[] = {0, 0}

#define LIMINE_BASE_REVISION(n) \
    __attribute__((used, section(".limine_requests"))) \
    static volatile uint64_t _limine_base_revision[] = { \
        0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (n) \
    }

#endif
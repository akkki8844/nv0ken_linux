#include <limine.h>

#include "arch/x86_64/cpu.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/irq.h"
#include "arch/x86_64/paging.h"
#include "drivers/acpi.h"
#include "drivers/framebuffer.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/pci.h"
#include "drivers/serial.h"
#include "drivers/timer.h"
#include "fs/fd_table.h"
#include "fs/initrd.h"
#include "fs/tmpfs.h"
#include "fs/vfs.h"
#include "ipc/shm.h"
#include "lib/kprintf.h"
#include "mm/addr_space.h"
#include "mm/heap.h"
#include "mm/mmap.h"
#include "mm/pmm.h"
#include "net/arp.h"
#include "net/netdev.h"
#include "net/socket.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "proc/syscall_table.h"

LIMINE_BASE_REVISION(2);

LIMINE_REQUEST static volatile struct limine_stack_size_request stack_size_request = {
    .id = { LIMINE_STACK_SIZE_REQUEST_ID },
    .revision = 0,
    .response = 0,
    .stack_size = 64 * 1024,
};

LIMINE_REQUEST static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = { LIMINE_FRAMEBUFFER_REQUEST_ID },
    .revision = 0,
    .response = 0,
};

LIMINE_REQUEST static volatile struct limine_memmap_request memmap_request = {
    .id = { LIMINE_MEMMAP_REQUEST_ID },
    .revision = 0,
    .response = 0,
};

LIMINE_REQUEST static volatile struct limine_hhdm_request hhdm_request = {
    .id = { LIMINE_HHDM_REQUEST_ID },
    .revision = 0,
    .response = 0,
};

LIMINE_REQUEST static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = { LIMINE_KERNEL_ADDRESS_REQUEST_ID },
    .revision = 0,
    .response = 0,
};

LIMINE_REQUEST static volatile struct limine_rsdp_request rsdp_request = {
    .id = { LIMINE_RSDP_REQUEST_ID },
    .revision = 0,
    .response = 0,
};

LIMINE_REQUEST static volatile struct limine_module_request module_request = {
    .id = { LIMINE_MODULE_REQUEST_ID },
    .revision = 0,
    .response = 0,
};

LIMINE_REQUEST_TERMINATE;

static unsigned char bootstrap_heap[1024 * 1024] __attribute__((aligned(16)));
static addr_space_t kernel_address_space;

static void halt_forever(void)
{
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static void init_memory(void)
{
    heap_init(bootstrap_heap, sizeof(bootstrap_heap));

    struct limine_memmap_response *memmap = memmap_request.response;
    if (memmap) {
        mmap_init_from_limine(memmap);
        pmm_init(mmap_highest_addr());
        for (size_t index = 0; index < mmap_region_count(); ++index) {
            const mmap_region_t *region = mmap_region_at(index);
            if (region->type == MMAP_REGION_USABLE) {
                pmm_mark_usable(region->base, region->length);
            } else {
                pmm_mark_reserved(region->base, region->length);
            }
        }
    } else {
        pmm_init(0);
    }

    uint64_t hhdm_offset = hhdm_request.response ? hhdm_request.response->offset : 0;
    paging_init(0);
    addr_space_init_kernel(&kernel_address_space, paging_current_cr3(), hhdm_offset);
}

static void init_filesystems(void)
{
    vfs_node_t *root = tmpfs_create_root();
    if (root) {
        vfs_mount_root(root);
        vfs_create("/dev", VFS_NODE_DIR, 0);
        vfs_create("/tmp", VFS_NODE_DIR, 0);
        vfs_create("/proc", VFS_NODE_DIR, 0);

        if (module_request.response && module_request.response->module_count > 0) {
            struct limine_file *module = module_request.response->modules[0];
            if (module && module->address && module->size) {
                initrd_load(module->address, (size_t)module->size, root);
            }
        }
    }
    fd_table_init();
}

static void init_kernel_services(void)
{
    shm_init();
    process_system_init();
    process_t *kernel_process = process_create("kernel", 0);
    if (kernel_process) {
        kernel_process->state = PROCESS_RUNNING;
        process_set_current(kernel_process);
    }
    scheduler_init();
    syscall_table_init();
    netdev_init();
    arp_init();
    socket_init();
}

static void init_hardware(void)
{
    timer_init(100);
    keyboard_init();
    mouse_init();
    if (rsdp_request.response) {
        acpi_init(rsdp_request.response->address);
    }
    pci_scan(0, 0);
    cpu_sti();
}

void kmain(void)
{
    cpu_init();
    serial_init(COM1);
    gdt_init();
    idt_init();
    irq_init();

    struct limine_framebuffer_response *fb_response = framebuffer_request.response;
    if (fb_response && fb_response->framebuffer_count > 0) {
        framebuffer_init(fb_response->framebuffers[0]);
    }

    framebuffer_clear(0x101820);

    init_memory();
    init_filesystems();
    init_kernel_services();
    init_hardware();

    kprintf("nv0ken_linux booted\n");
    kprintf("limine framebuffer: %ux%u\n",
            framebuffer_width(),
            framebuffer_height());
    kprintf("x86_64 descriptors and irq layer online\n");
    kprintf("memory: regions=%u free_frames=%u heap_free=%u\n",
            (unsigned)mmap_region_count(),
            (unsigned)pmm_free_count(),
            (unsigned)heap_bytes_free());
    kprintf("vfs/tmpfs, ipc, scheduler, syscalls, and net core online\n");

    halt_forever();
}

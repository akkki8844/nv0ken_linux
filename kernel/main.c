#include <limine.h>

#include "arch/x86_64/cpu.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/irq.h"
#include "arch/x86_64/paging.h"
#include "boot/multiboot2.h"
#include "drivers/acpi.h"
#include "drivers/console.h"
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
#include "lib/klog.h"
#include "mm/addr_space.h"
#include "mm/heap.h"
#include "mm/mmap.h"
#include "mm/pmm.h"
#include "monitor.h"
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
static unsigned pci_device_count;
extern char __kernel_virtual_end[];

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
    } else if (multiboot2_available()) {
        multiboot2_init_memory_map();
    } else {
        mmap_reset();
    }

    if (mmap_region_count() > 0) {
        pmm_init(mmap_highest_addr());
        for (size_t index = 0; index < mmap_region_count(); ++index) {
            const mmap_region_t *region = mmap_region_at(index);
            if (region->type == MMAP_REGION_USABLE) {
                pmm_mark_usable(region->base, region->length);
            } else {
                pmm_mark_reserved(region->base, region->length);
            }
        }
        uint64_t kernel_start = 0x100000;
        uint64_t kernel_end =
            (uint64_t)(uintptr_t)__kernel_virtual_end - 0xffffffff80000000ull;
        pmm_mark_reserved(0, 0x100000);
        pmm_mark_reserved(kernel_start, kernel_end - kernel_start);

        const void *module_address;
        size_t module_size;
        if (multiboot2_module(0, &module_address, &module_size)) {
            pmm_mark_reserved((uint64_t)(uintptr_t)module_address, module_size);
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
        vfs_create("/dev/klog", VFS_NODE_FILE, 0);

        if (module_request.response && module_request.response->module_count > 0) {
            struct limine_file *module = module_request.response->modules[0];
            if (module && module->address && module->size) {
                initrd_load(module->address, (size_t)module->size, root);
            }
        } else if (multiboot2_available()) {
            const void *module_address;
            size_t module_size;
            if (multiboot2_module(0, &module_address, &module_size)) {
                initrd_load(module_address, module_size, root);
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

static void pci_boot_probe(const pci_device_t *device, void *context)
{
    (void)context;
    if (!device) {
        return;
    }
    ++pci_device_count;
    kprintf("[pci] %u:%u.%u vendor=%x device=%x class=%x:%x\n",
            device->bus,
            device->slot,
            device->function,
            device->vendor_id,
            device->device_id,
            device->class_code,
            device->subclass);
}

static void init_hardware(void)
{
    timer_init(100);
    console_init();
    keyboard_init();
    mouse_init();
    if (rsdp_request.response) {
        acpi_init(rsdp_request.response->address);
    } else {
        const void *rsdp = multiboot2_rsdp();
        if (rsdp) {
            acpi_init((void *)rsdp);
        }
    }
    pci_device_count = 0;
    pci_scan(pci_boot_probe, 0);
    cpu_sti();
}

void kmain(void)
{
    klog_clear();
    cpu_init();
    serial_init(COM1);
    gdt_init();
    idt_init();
    irq_init();

    struct limine_framebuffer_response *fb_response = framebuffer_request.response;
    if (fb_response && fb_response->framebuffer_count > 0) {
        framebuffer_init(fb_response->framebuffers[0]);
    } else {
        multiboot2_framebuffer_t framebuffer;
        if (multiboot2_framebuffer(&framebuffer)) {
            framebuffer_init_raw(framebuffer.address,
                                 framebuffer.width,
                                 framebuffer.height,
                                 framebuffer.pitch,
                                 framebuffer.bpp);
        }
    }

    framebuffer_clear(0x101820);

    init_memory();
    init_filesystems();
    init_kernel_services();
    init_hardware();

    kprintf("\n+--------------------------------------------------+\n");
    kprintf("|                nv0ken_linux 0.1                 |\n");
    kprintf("|        x86_64 educational operating system      |\n");
    kprintf("+--------------------------------------------------+\n");
    kprintf("[boot] framebuffer %ux%u\n",
            framebuffer_width(),
            framebuffer_height());
    kprintf("[boot] protocol=%s\n",
            multiboot2_available() ? "multiboot2" : "limine");
    if (kernel_address_request.response) {
        kprintf("[boot] kernel phys=%llx virt=%llx\n",
                (unsigned long long)kernel_address_request.response->physical_base,
                (unsigned long long)kernel_address_request.response->virtual_base);
    }
    kprintf("[arch] descriptors, interrupts, syscall/sysret online\n");
    kprintf("[mem ] regions=%zu free_frames=%zu heap_free=%zu bytes\n",
            mmap_region_count(), pmm_free_count(), heap_bytes_free());
    kprintf("[fs  ] tmpfs root and initrd mounted; /dev/klog ready\n");
    kprintf("[hw  ] timer, PS/2 input, ACPI, PCI (%u devices) online\n",
            pci_device_count);
    kprintf("[net ] ARP, IPv4, ICMP, UDP, TCP and sockets initialized\n");
    kprintf("[ ok ] kernel initialization complete\n\n");

    monitor_run(pci_device_count);
    halt_forever();
}

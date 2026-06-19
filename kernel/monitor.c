#include "monitor.h"

#include "arch/x86_64/cpu.h"
#include "arch/x86_64/io.h"
#include "drivers/console.h"
#include "drivers/framebuffer.h"
#include "drivers/serial.h"
#include "drivers/timer.h"
#include "fs/vfs.h"
#include "lib/klog.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "mm/pmm.h"

#define MONITOR_LINE_MAX 160

static unsigned monitor_pci_devices;

static int starts_with(const char *text, const char *prefix)
{
    while (*prefix) {
        if (*text++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static void command_help(void)
{
    kputs("commands:\n");
    kputs("  help          show this command list\n");
    kputs("  status        show kernel subsystem status\n");
    kputs("  mem           show physical and heap memory\n");
    kputs("  uptime        show PIT uptime\n");
    kputs("  ls            list the root filesystem\n");
    kputs("  cat PATH      print a file from the VFS\n");
    kputs("  dmesg         print the in-memory kernel log\n");
    kputs("  echo TEXT     print text\n");
    kputs("  clear         clear the framebuffer console\n");
    kputs("  reboot        reset through the PS/2 controller\n");
    kputs("  halt          stop the CPU\n");
}

static void command_status(void)
{
    kprintf("kernel : nv0ken_linux 0.1 x86_64\n");
    kprintf("memory : %zu free / %zu total frames\n", pmm_free_count(), pmm_total_frames());
    kprintf("heap   : %zu free / %zu used bytes\n", heap_bytes_free(), heap_bytes_used());
    kprintf("pci    : %u devices discovered\n", monitor_pci_devices);
    kprintf("vfs    : %s\n", vfs_root() ? "mounted" : "offline");
    kprintf("klog   : %zu bytes retained\n", klog_size());
}

static void command_ls(void)
{
    vfs_node_t *root = vfs_root();
    if (!root) {
        kputs("vfs is not mounted\n");
        return;
    }
    for (vfs_node_t *node = root->children; node; node = node->next) {
        kprintf("%c %s\n", node->type == VFS_NODE_DIR ? 'd' : '-', node->name);
    }
}

static void command_cat(const char *path)
{
    vfs_node_t *node = vfs_lookup(path);
    if (!node || node->type != VFS_NODE_FILE) {
        kprintf("cat: not a file: %s\n", path);
        return;
    }

    char buffer[128];
    size_t offset = 0;
    for (;;) {
        int read = vfs_read(node, offset, buffer, sizeof(buffer));
        if (read <= 0) {
            break;
        }
        for (int index = 0; index < read; ++index) {
            kputchar(buffer[index]);
        }
        offset += (size_t)read;
    }
    if (offset == 0 || buffer[(offset - 1) % sizeof(buffer)] != '\n') {
        kputchar('\n');
    }
}

static void command_dmesg(void)
{
    char buffer[128];
    size_t offset = 0;
    size_t snapshot = klog_size();
    while (offset < snapshot) {
        size_t read = klog_read(offset, buffer, sizeof(buffer));
        if (read == 0) {
            break;
        }
        for (size_t index = 0; index < read; ++index) {
            serial_write_char(buffer[index]);
            framebuffer_putc(buffer[index]);
        }
        offset += read;
    }
}

static void execute_command(char *line)
{
    if (!line[0]) {
        return;
    }
    if (strcmp(line, "help") == 0) {
        command_help();
    } else if (strcmp(line, "status") == 0) {
        command_status();
    } else if (strcmp(line, "mem") == 0) {
        kprintf("frames: total=%zu used=%zu free=%zu\n",
                pmm_total_frames(), pmm_used_count(), pmm_free_count());
        kprintf("heap: used=%zu free=%zu\n", heap_bytes_used(), heap_bytes_free());
    } else if (strcmp(line, "uptime") == 0) {
        kprintf("uptime: %llu ms (%llu ticks)\n",
                (unsigned long long)timer_uptime_ms(),
                (unsigned long long)timer_ticks());
    } else if (strcmp(line, "ls") == 0) {
        command_ls();
    } else if (starts_with(line, "cat ")) {
        command_cat(line + 4);
    } else if (strcmp(line, "dmesg") == 0) {
        command_dmesg();
    } else if (starts_with(line, "echo ")) {
        kprintf("%s\n", line + 5);
    } else if (strcmp(line, "clear") == 0) {
        framebuffer_clear(0x101820);
    } else if (strcmp(line, "reboot") == 0) {
        cpu_cli();
        outb(0x64, 0xfe);
        for (;;) {
            cpu_halt();
        }
    } else if (strcmp(line, "halt") == 0) {
        kputs("system halted\n");
        cpu_cli();
        for (;;) {
            cpu_halt();
        }
    } else {
        kprintf("unknown command: %s (try 'help')\n", line);
    }
}

void monitor_run(unsigned pci_devices)
{
    monitor_pci_devices = pci_devices;
    kputs("nv0 monitor ready. type 'help'.\n");

    for (;;) {
        char line[MONITOR_LINE_MAX];
        kputs("nv0> ");
        size_t length = console_read(line, sizeof(line) - 1, 1);
        if (length > 0 && line[length - 1] == '\n') {
            --length;
        }
        line[length] = '\0';
        execute_command(line);
    }
}

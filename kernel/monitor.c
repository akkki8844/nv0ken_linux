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
    kputs("nv0 monitor commands:\n");
    kputs("  help                 show this command list\n");
    kputs("  status | mem | uptime system health and resource information\n");
    kputs("  ls [PATH]             list a directory\n");
    kputs("  tree [PATH]           list a directory tree\n");
    kputs("  cat PATH              print a VFS file\n");
    kputs("  write PATH TEXT       create or replace a VFS file\n");
    kputs("  mkdir PATH | rm PATH  create or remove an empty entry\n");
    kputs("  selftest              verify VFS read/write/truncate/remove\n");
    kputs("  dmesg | echo TEXT | clear\n");
    kputs("  reboot | halt\n");
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

static const char *node_type_name(vfs_node_type_t type)
{
    switch (type) {
    case VFS_NODE_DIR: return "dir";
    case VFS_NODE_FILE: return "file";
    case VFS_NODE_DEVICE: return "device";
    case VFS_NODE_PIPE: return "pipe";
    }
    return "unknown";
}

static void command_ls(const char *path)
{
    vfs_node_t *directory = vfs_lookup(path && path[0] ? path : "/");
    if (!directory || directory->type != VFS_NODE_DIR) {
        kprintf("ls: not a directory: %s\n", path && path[0] ? path : "/");
        return;
    }
    for (vfs_node_t *node = directory->children; node; node = node->next) {
        kprintf("%s %llu bytes  %s\n", node_type_name(node->type),
                (unsigned long long)node->size, node->name);
    }
}

static void command_tree_node(vfs_node_t *node, unsigned depth, unsigned *shown)
{
    if (!node || *shown >= 128 || depth > 8) {
        return;
    }
    for (unsigned i = 0; i < depth; ++i) {
        kputs("  ");
    }
    kprintf("%s%s\n", node->name, node->type == VFS_NODE_DIR ? "/" : "");
    ++*shown;
    if (node->type == VFS_NODE_DIR) {
        for (vfs_node_t *child = node->children; child; child = child->next) {
            command_tree_node(child, depth + 1, shown);
        }
    }
}

static void command_tree(const char *path)
{
    vfs_node_t *node = vfs_lookup(path && path[0] ? path : "/");
    if (!node) {
        kprintf("tree: not found: %s\n", path && path[0] ? path : "/");
        return;
    }
    unsigned shown = 0;
    command_tree_node(node, 0, &shown);
    if (shown >= 128) {
        kputs("tree: output capped at 128 entries\n");
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
    char last = '\0';
    for (;;) {
        int read = vfs_read(node, offset, buffer, sizeof(buffer));
        if (read <= 0) {
            break;
        }
        for (int index = 0; index < read; ++index) {
            kputchar(buffer[index]);
        }
        last = buffer[read - 1];
        offset += (size_t)read;
    }
    if (offset == 0 || last != '\n') {
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

static void command_write(const char *path, const char *text)
{
    if (!path || !path[0] || !text) {
        kputs("usage: write PATH TEXT\n");
        return;
    }
    vfs_node_t *node = vfs_lookup(path);
    if (!node && vfs_create(path, VFS_NODE_FILE, &node) != 0) {
        kprintf("write: cannot create: %s\n", path);
        return;
    }
    if (!node || node->type != VFS_NODE_FILE || !node->ops ||
        !node->ops->truncate || node->ops->truncate(node, 0) != 0 ||
        vfs_write(node, 0, text, strlen(text)) != (int)strlen(text)) {
        kprintf("write: failed: %s\n", path);
        return;
    }
    kprintf("wrote %zu bytes to %s\n", strlen(text), path);
}

static void command_mkdir(const char *path)
{
    if (!path || !path[0] || vfs_create(path, VFS_NODE_DIR, 0) != 0) {
        kputs("mkdir: failed (path may exist or parent may be missing)\n");
        return;
    }
    kprintf("created %s\n", path);
}

static void command_rm(const char *path)
{
    if (!path || !path[0] || vfs_unlink(path) != 0) {
        kputs("rm: failed (root and non-empty directories are protected)\n");
        return;
    }
    kprintf("removed %s\n", path);
}

static int monitor_selftest(void)
{
    static const char path[] = "/tmp/.nv0-selftest";
    static const char payload[] = "nv0 filesystem self-test";
    char buffer[sizeof(payload)];
    vfs_node_t *node = vfs_lookup(path);
    if (node) {
        (void)vfs_unlink(path);
    }
    if (vfs_create(path, VFS_NODE_FILE, &node) != 0 || !node ||
        vfs_write(node, 0, payload, sizeof(payload) - 1) != (int)(sizeof(payload) - 1) ||
        vfs_read(node, 0, buffer, sizeof(payload) - 1) != (int)(sizeof(payload) - 1) ||
        memcmp(buffer, payload, sizeof(payload) - 1) != 0 || !node->ops ||
        !node->ops->truncate || node->ops->truncate(node, 0) != 0 || node->size != 0 ||
        vfs_unlink(path) != 0 || vfs_lookup(path)) {
        kputs("[selftest] FAIL: VFS read/write/truncate/remove\n");
        return -1;
    }
    kputs("[selftest] PASS: VFS read/write/truncate/remove\n");
    return 0;
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
        command_ls("/");
    } else if (starts_with(line, "ls ")) {
        command_ls(line + 3);
    } else if (strcmp(line, "tree") == 0) {
        command_tree("/");
    } else if (starts_with(line, "tree ")) {
        command_tree(line + 5);
    } else if (starts_with(line, "cat ")) {
        command_cat(line + 4);
    } else if (starts_with(line, "write ")) {
        char *path = line + 6;
        char *text = path;
        while (*text && *text != ' ') {
            ++text;
        }
        if (*text) {
            *text++ = '\0';
        }
        command_write(path, *text ? text : 0);
    } else if (starts_with(line, "mkdir ")) {
        command_mkdir(line + 6);
    } else if (starts_with(line, "rm ")) {
        command_rm(line + 3);
    } else if (strcmp(line, "selftest") == 0) {
        (void)monitor_selftest();
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
    kputs("\n=== nv0ken recovery environment ===\n");
    (void)monitor_selftest();
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

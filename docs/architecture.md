# nv0ken_linux architecture

## overview

nv0ken_linux is a monolithic 64-bit x86_64 operating system built from scratch. the kernel runs in ring 0, userspace in ring 3. there is no microkernel separation — drivers, the scheduler, the VFS, and the network stack all live in kernel space.

## address space layout

```
0xFFFFFFFF80000000  kernel image (linked at -2GB)
0xFFFF800000000000  higher half direct map (HHDM) — full physical memory mapped here
0x0000800000000000  end of userspace
0x0000000000400000  userspace ELF load base
0x0000000000000000  null guard page
```

the kernel is linked at the higher half. limine maps the kernel before handing control. the HHDM offset is retrieved from the limine HHDM request and stored in a global used by the PMM and VMM to convert physical addresses to kernel-accessible virtual addresses.

## subsystem interaction

```
limine bootloader
    |
    v
kernel/arch/x86_64/boot_entry.asm   <- sets RSP, clears BSS, calls kmain
    |
    v
kernel/main.c (kmain)
    |
    +-- mm/mmap.c        parse limine memory map
    +-- mm/pmm.c         init physical frame allocator
    +-- mm/vmm.c         init page tables
    +-- mm/heap.c        init kernel heap (kmalloc)
    +-- arch/x86_64/gdt.c
    +-- arch/x86_64/idt.c
    +-- arch/x86_64/pic.c
    +-- arch/x86_64/tss.c
    +-- drivers/serial.c     debug output
    +-- drivers/framebuffer.c
    +-- lib/kprintf.c        kernel printf → FB + serial
    +-- drivers/timer.c      PIT IRQ0 → scheduler tick
    +-- drivers/keyboard.c   PS/2 IRQ1
    +-- drivers/acpi.c       RSDP/MADT parse
    +-- arch/x86_64/apic.c   LAPIC init
    +-- fs/vfs.c
    +-- fs/initrd.c          mount initrd as root
    +-- proc/scheduler.c     init scheduler
    +-- proc/process.c       spawn PID 1 (init)
```

## kernel subsystems

### mm — memory management

three-layer stack. PMM manages physical 4KB frames using a bitmap. VMM manages 4-level page tables (PML4→PDPT→PD→PT) and provides `vmm_map`, `vmm_unmap`, `vmm_clone` for per-process address spaces. heap.c provides `kmalloc`/`kfree`/`krealloc` built on top of VMM using a linked-list free block allocator.

### proc — process management

each process has a PCB (`process_t`) containing its register state, page table root (CR3), file descriptor table, signal mask, and scheduler queue links. `scheduler.c` runs a round-robin tick-driven preemptive scheduler. `elf_loader.c` parses ELF64 binaries and maps PT_LOAD segments into the new process address space. `syscall_table.c` is a flat array of function pointers indexed by syscall number, invoked from the SYSCALL trampoline in `arch/x86_64/syscall.asm`.

### fs — virtual filesystem

VFS defines a uniform inode/dentry/file interface. concrete filesystems register with `vfs_register`. all syscalls (open, read, write, stat, readdir) go through VFS ops tables. ext2 and tmpfs are the two concrete implementations. initrd is a tar archive loaded by limine as a module and mounted as the initial root.

### drivers

drivers register IRQ handlers via `irq_register`. the PIC is remapped to vectors 32–47. drivers access hardware through `io.h` port macros (inb/outb). the framebuffer driver writes directly to the physical framebuffer address provided by limine. the PIT fires at a configurable rate (default 100Hz) and calls `scheduler_tick` on every IRQ0.

### net — networking

layered stack: RTL8139 NIC driver → ethernet → ARP → IPv4 → TCP/UDP → socket. the socket layer exposes a BSD-compatible API to userspace via syscalls. the network buffer (`net_buf`) is a heap-allocated chain of data segments passed up and down the stack.

## boot sequence

1. BIOS/UEFI loads Limine from the ISO
2. Limine loads `kernel.elf`, maps the kernel to the higher half, fills in request response pointers
3. Limine jumps to `_start` in `boot_entry.asm`
4. `boot_entry.asm` sets up a 64KB initial stack, zeroes BSS, calls `kmain`
5. `kmain` initialises all subsystems in dependency order
6. `kmain` spawns PID 1 (`/bin/init`) and calls `scheduler_start`
7. the scheduler begins switching between processes via IRQ0

## inter-subsystem dependencies

```
heap    depends on  vmm
vmm     depends on  pmm
pmm     depends on  mmap (limine memory map)
proc    depends on  mm, fs, arch
fs      depends on  mm
net     depends on  mm, drivers
drivers depend on   arch (IRQ, port I/O)
```
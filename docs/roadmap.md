# roadmap

## phase 1 — boot and framebuffer

goal: kernel boots via limine, draws to the screen, outputs debug text.

- [x] limine.conf and limine.h
- [x] kernel/arch/x86_64/boot_entry.asm
- [x] kernel/arch/x86_64/gdt.c/h
- [x] kernel/arch/x86_64/idt.c/h + isr.asm
- [x] kernel/arch/x86_64/pic.c/h
- [x] kernel/arch/x86_64/io.h
- [x] kernel/drivers/serial.c/h
- [x] kernel/drivers/framebuffer.c/h
- [x] kernel/lib/kprintf.c/h
- [x] kernel/lib/string.c/h
- [x] kernel/lib/types.h
- [x] kernel/lib/panic.c/h
- [x] kernel/main.c (stub — GDT, IDT, FB, kprintf, halt)
- [x] kernel/kernel.ld
- [x] top-level Makefile + toolchain.mk
- [x] tools/mkiso.sh + tools/run_qemu.sh

milestone: `qemu-system-x86_64 -cdrom nv0ken.iso` shows "nv0ken_linux booted" on screen.

## phase 2 — memory management

goal: kernel can dynamically allocate and free memory.

- [ ] kernel/mm/mmap.c/h (parse limine memory map)
- [ ] kernel/mm/pmm.c/h (bitmap physical frame allocator)
- [ ] kernel/arch/x86_64/paging.c/h (4-level page table setup)
- [ ] kernel/mm/vmm.c/h (map/unmap/clone virtual address spaces)
- [ ] kernel/mm/heap.c/h (kmalloc/kfree/krealloc)
- [ ] kernel/lib/bitmap.c/h
- [ ] kernel/lib/spinlock.c/h

milestone: `kmalloc(4096)` and `kfree` work, PMM bitmap covers all physical RAM shown in QEMU info.

## phase 3 — processes and userspace

goal: kernel spawns PID 1, userspace programs run in ring 3, system calls work.

- [ ] kernel/arch/x86_64/tss.c/h
- [ ] kernel/arch/x86_64/syscall.asm + syscall.c
- [ ] kernel/arch/x86_64/context.asm
- [ ] kernel/drivers/timer.c/h (PIT IRQ0)
- [ ] kernel/proc/process.c/h
- [ ] kernel/proc/thread.c/h
- [ ] kernel/proc/scheduler.c/h
- [ ] kernel/proc/elf_loader.c/h
- [ ] kernel/proc/syscall_table.c/h
- [ ] kernel/proc/syscall_handlers.c/h
- [ ] kernel/proc/exec.c/h
- [ ] kernel/proc/wait.c/h
- [ ] kernel/fs/vfs.c/h
- [ ] kernel/fs/tmpfs.c/h
- [ ] kernel/fs/initrd.c/h
- [ ] kernel/ipc/pipe.c/h
- [ ] kernel/ipc/signal.c/h
- [ ] userland/libc/ (crt0, stdio, stdlib, string, syscall.asm)
- [ ] userland/init/init.c
- [ ] userland/shell/ (nv0sh)
- [ ] userland/utils/ (ls, cat, echo, mkdir, rm, ps, etc.)
- [ ] tools/pack_initrd.sh

milestone: boot to shell prompt. `ls /`, `cat /etc/motd`, `echo hello` work.

## phase 4 — filesystem

goal: persistent storage, ext2 read/write, file operations work end-to-end.

- [ ] kernel/drivers/ata.c/h (ATA PIO)
- [ ] kernel/drivers/pci.c/h
- [ ] kernel/fs/ext2.c/h
- [ ] kernel/fs/fd_table.c/h
- [ ] kernel/fs/path.c/h
- [ ] tools/mkfs_ext2.sh

milestone: mount ext2 disk image, read and write files, survive reboot with data intact.

## phase 5 — graphics and desktop

goal: graphical desktop environment runs, windows open, apps launch.

- [ ] kernel/drivers/keyboard.c/h (PS/2 IRQ1)
- [ ] kernel/drivers/mouse.c/h (PS/2 IRQ12)
- [ ] kernel/ipc/shm.c/h
- [ ] libnv0/ (window, draw, input, app, dialog headers + src)
- [ ] graphics/server/ (compositor, surface, damage, event_loop, cursor)
- [ ] graphics/wm/ (window manager)
- [ ] graphics/font/ (PSF2 loader + renderer)
- [ ] graphics/draw/ (2D primitives)
- [ ] graphics/widgets/ (button, label, textbox, etc.)
- [ ] graphics/desktop/ (taskbar, launcher, wallpaper)
- [ ] apps/terminal/ (vt100, pty, main)
- [ ] apps/text_editor/ (buffer, editor, syntax, main)
- [ ] apps/file_manager/ (dir_view, ops, main)
- [ ] apps/image_viewer/ (decode_bmp, decode_ppm, main)
- [ ] apps/calculator/ (main)
- [ ] apps/browser/ (html_parser, http_client, renderer, main)

milestone: desktop loads, terminal opens, text editor opens a file, file manager browses.

## phase 6 — networking

goal: TCP/IP stack works, browser can fetch a real HTTP page.

- [ ] kernel/drivers/rtl8139.c/h
- [ ] kernel/net/netdev.c/h
- [ ] kernel/net/ethernet.c/h
- [ ] kernel/net/arp.c/h
- [ ] kernel/net/ipv4.c/h
- [ ] kernel/net/icmp.c/h
- [ ] kernel/net/udp.c/h
- [ ] kernel/net/tcp.c/h
- [ ] kernel/net/socket.c/h
- [ ] kernel/net/dhcp.c/h
- [ ] kernel/net/dns.c/h

milestone: `ping 8.8.8.8` works in QEMU, browser fetches `http://example.com`.

## phase 7 — stability and polish

goal: system is usable for extended sessions without crashing.

- [ ] kernel/mm/cow.c/h (copy-on-write fork)
- [ ] kernel/mm/slab.c/h (slab allocator)
- [ ] kernel/arch/x86_64/apic.c/h (replace PIC with LAPIC)
- [ ] kernel/arch/x86_64/smp.c/h (multi-core AP bringup)
- [ ] kernel/ipc/semaphore.c/h
- [ ] kernel/ipc/mutex.c/h
- [ ] kernel/ipc/msgqueue.c/h
- [ ] kernel/drivers/acpi.c/h
- [ ] kernel/drivers/cmos.c/h
- [ ] improved scheduler (priority + sleep queues)
- [ ] apps/browser/ (full HTML/CSS rendering)
- [ ] docs/ (complete all documentation)
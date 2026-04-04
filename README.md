# nv0ken_linux

a 64-bit x86_64 operating system built from scratch. boots via the limine bootloader, runs a graphical desktop environment, and includes a TCP/IP networking stack, a virtual filesystem, a preemptive scheduler, and a suite of desktop applications.

---

## quick start

```bash
# install dependencies
sudo apt install build-essential nasm qemu-system-x86 xorriso git

# clone
git clone https://github.com/yourname/nv0ken_linux.git
cd nv0ken_linux

# build and run
bash build.sh run
```

that's it. the script fetches the limine bootloader automatically on first run.

---

## what's inside

### kernel
- 64-bit x86_64 monolithic kernel
- 4-level paging with higher-half mapping
- physical memory manager (bitmap allocator)
- virtual memory manager with per-process address spaces
- kernel heap (`kmalloc`/`kfree`)
- preemptive round-robin scheduler
- ELF64 binary loader
- system calls via `SYSCALL`/`SYSRET`
- PS/2 keyboard and mouse drivers
- PIT timer, PIC, Local APIC
- ATA PIO disk driver
- RTL8139 NIC driver
- ACPI RSDP/MADT parsing
- VFS layer with ext2 and tmpfs
- TCP/IP stack (ethernet → ARP → IPv4 → TCP/UDP → sockets)
- POSIX signals, pipes, shared memory, semaphores, message queues

### userland
- minimal libc (stdio, stdlib, string, ctype, unistd, signal)
- PID 1 init with inittab service management
- nv0sh — a POSIX shell with pipelines, redirects, job control, history, and tab expansion
- 29 core utilities (ls, cat, grep, find, cp, mv, ps, kill, hexdump, and more)

### graphics
- limine framebuffer compositor
- window manager with decorations, focus, and z-order
- PSF2 bitmap font renderer
- 2D drawing primitives with alpha blending
- UI widget toolkit (button, label, textbox, listview, menu, scrollbar)
- desktop environment with taskbar and launcher

### applications
- terminal emulator (VT100/xterm-256color, PTY)
- text editor (gap buffer, syntax highlighting for C, Python, JS, shell, ASM, Markdown, JSON)
- file manager (directory listing, copy/move/delete, bookmarks)
- image viewer (BMP and PPM/PGM/PBM/PAM decoders, zoom, pan, rotate)
- calculator (arithmetic, sqrt, x², 1/x, history)
- web browser (HTTP/1.1 client, HTML parser, basic renderer)

---

## build targets

| command | effect |
|---|---|
| `bash build.sh` | build everything + ISO |
| `bash build.sh run` | build everything + launch QEMU |
| `bash build.sh run-debug` | build everything + QEMU with GDB stub on :1234 |
| `bash build.sh kernel` | kernel only |
| `bash build.sh userland` | libc + init + shell + utils |
| `bash build.sh apps` | all desktop apps |
| `bash build.sh clean` | remove build/ |
| `bash build.sh -j8 run` | parallel build with 8 jobs |
| `bash build.sh --kvm run` | run with KVM acceleration |

or use make directly:

```bash
make          # build everything
make run      # build + QEMU
make kernel   # kernel only
make clean    # wipe build/
```

---

## project layout

```
nv0ken_linux/
├── boot/           limine config and protocol header
├── kernel/         kernel source — arch, mm, proc, drivers, fs, net, ipc, lib
├── userland/       ring-3 programs — libc, init, shell, utils
├── graphics/       compositor, WM, font renderer, widget toolkit, desktop
├── libnv0/         shared userspace library (window, draw, input, IPC)
├── apps/           desktop applications
├── tools/          build scripts, QEMU launchers, debug helpers
├── assets/         fonts, icons, wallpapers, cursors
├── docs/           architecture, memory layout, syscall table, roadmap
└── build/          generated output (not committed)
```

---

## debugging

attach GDB to a running QEMU instance:

```bash
# terminal 1
bash build.sh run-debug

# terminal 2
gdb -x tools/gdbinit
```

useful GDB commands after attaching:

```
break kmain
continue
bt
info registers
x/20i $rip
```

serial output (all `kprintf` calls) appears in the terminal where QEMU was launched.

---

## roadmap

- [x] phase 1 — boot, framebuffer, serial debug output
- [ ] phase 2 — physical and virtual memory management
- [ ] phase 3 — processes, scheduler, syscalls, VFS, userland shell
- [ ] phase 4 — ext2 filesystem, persistent storage
- [ ] phase 5 — graphics, desktop environment, applications
- [ ] phase 6 — TCP/IP networking
- [ ] phase 7 — SMP, APIC, COW fork, slab allocator, stability

see `docs/roadmap.md` for the full task checklist.

---

## documentation

| file | contents |
|---|---|
| `docs/architecture.md` | kernel subsystem map and boot sequence |
| `docs/memory_layout.md` | virtual address space diagram and page table layout |
| `docs/syscall_table.md` | all 77 syscalls with signatures and errno values |
| `docs/driver_model.md` | how to write a driver, IRQ registration, PCI probing |
| `docs/vfs_interface.md` | VFS ops table, how to implement a filesystem |
| `docs/ipc_protocol.md` | compositor IPC message format and connection lifecycle |
| `docs/building.md` | toolchain setup and build instructions |
| `docs/debugging.md` | GDB workflow, QEMU monitor, common boot failures |
| `docs/roadmap.md` | phase-by-phase development plan with checkboxes |

---

## license

this project is for educational purposes.
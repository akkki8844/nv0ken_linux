🚀 nv0ken Linux

A custom Unix-like operating system built entirely from scratch in C, targeting x86_64 architecture.

MyOS is a long-term systems engineering project focused on building a modern graphical operating system with a custom kernel, memory manager, scheduler, userspace, and desktop environment.

🌍 Vision

The goal of MyOS is to design and implement a fully self-contained operating system stack:

Custom kernel

Custom memory management

Custom process model

Custom graphics stack

Custom window manager

Custom userland

This project is inspired by systems like:

Linux (kernel architecture)

SerenityOS (full-stack ownership)

Redox OS (modern design principles)

The objective is not to fork Linux, but to build an operating system from first principles.

🧠 Architecture Overview

MyOS is structured into modular subsystems:

MyOS/
├── Boot/              # Limine bootloader integration
├── Kernel/
│   ├── Memory/        # Physical & virtual memory management
│   ├── Scheduler/     # Task switching & scheduling
│   ├── Drivers/       # Hardware drivers
│   ├── Graphics/      # Framebuffer & rendering
│   └── Syscalls/      # Kernel interface
├── Userland/          # Shell & system utilities
├── LibC/              # Custom C standard library
├── Include/           # Shared headers
├── build.sh           # Build system
└── README.md
🔧 Current Status
Phase 1 — Kernel Foundation

 Limine boot integration

 Framebuffer graphics

 Physical memory manager

 Virtual memory (paging)

 Heap allocator

 Interrupt descriptor table

 Timer interrupt

 Basic scheduler

Phase 2 — Userspace

 ELF loader

 Syscall interface

 Minimal libc

 Shell

Phase 3 — Desktop Environment

 Window compositor

 Input handling (keyboard & mouse)

 Basic GUI toolkit

 Window manager

🖥 Features (Planned)

64-bit kernel

Preemptive multitasking

Virtual memory

Hardware abstraction layer

Framebuffer-based GUI

Custom window manager

Filesystem support

Networking stack

Userspace process isolation

🛠 Building & Running
Requirements

GCC (with 64-bit support)

binutils

xorriso

QEMU

Limine bootloader

Build
./build.sh
Run
qemu-system-x86_64 -cdrom myos.iso
🎯 Long-Term Goals

Build a fully graphical desktop OS

Implement multi-process userspace

Develop native applications

Create a package management system

Support real hardware boot

📚 Learning Goals

This project explores:

Low-level systems programming

Operating system architecture

Memory management

Computer architecture

Toolchains & linking

Kernel debugging

Graphics rendering pipelines

🤝 Contributing

This is currently a personal systems engineering project.

Future contributors are welcome once the core architecture stabilizes.

⚠️ Disclaimer

MyOS is experimental software and is not intended for production use.

📜 License

MIT License (or choose your preferred license)
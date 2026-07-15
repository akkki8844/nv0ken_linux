# Product requirements document

## Product summary

nv0ken is an educational, from-scratch 64-bit x86_64 operating-system project.
It is built as a bootable ISO image using Limine, provides a kernel with a
serial and framebuffer diagnostic path, and includes a growing user-space,
graphics stack, and desktop application set.

The project is a learning and engineering platform, not a production operating
system. It must therefore prefer transparent failure modes, reproducible builds,
and testable subsystem boundaries over feature count alone.

## Users and use cases

| User | Need |
| --- | --- |
| OS developer | Build, boot, trace, and change a subsystem without undocumented setup. |
| Contributor | Understand ownership, interfaces, coding expectations, and review criteria. |
| Evaluator | Produce a bootable ISO and inspect the kernel's serial output in QEMU. |
| Learner | Follow the architecture from boot entry through kernel services and user programs. |

## Supported environment

- Target architecture: x86_64.
- Primary boot path: Limine ISO, including BIOS and UEFI media created by the packaging script.
- Primary emulator: QEMU x86_64.
- Host environment: a POSIX-like shell with the toolchain listed in [building.md](building.md).
- Kernel toolchain: a freestanding x86_64 ELF compiler, linker, assembler, and archive tool.

Hardware boot, SMP, persistent storage, network interoperability, and desktop
application completeness are tracked as engineering work rather than promised
release guarantees until validated on their intended target.

## Functional requirements

### Build and boot

1. A clean checkout must provide documented commands for building the kernel,
   userland, initrd, and ISO.
2. ISO packaging must reject a missing or non-ELF kernel rather than create a
   misleading image.
3. The boot image must include the kernel, initrd, Limine configuration, and
   required bootloader files.
4. A QEMU launch path must expose serial output for diagnosing early boot.

### Kernel foundation

1. The kernel must enter through the configured boot protocol and initialize a
   safe serial logging path before complex subsystem startup.
2. Panic and recovery paths must leave useful diagnostics on serial output.
3. Kernel/user ABI changes must be reflected in the syscall, IPC, or VFS
   reference documents as appropriate.

### User experience

1. The initrd must contain only the intended executable and runtime assets.
2. The project must retain a usable diagnostic/recovery route when desktop
   components are incomplete or fail to start.
3. Applications must fail cleanly when a required kernel capability is absent.

## Quality requirements

- Build scripts are deterministic: they create their output from tracked source
  and declared external tools, not untracked local binaries.
- Generated images and object files are not committed.
- New code compiles without introducing warnings in the modified component.
- Every change that affects boot packaging is tested by rebuilding the ISO and
  inspecting its contents.
- Every externally visible interface has a documented ownership and error
  contract before it becomes a dependency of another subsystem.

## Acceptance criteria for an integration milestone

An integration milestone is acceptable when all of the following are true:

1. `make iso` completes with the documented toolchain.
2. The generated kernel passes an ELF-header check.
3. The ISO contains the kernel, initrd, Limine configuration, and EFI boot file.
4. The ISO boots in headless QEMU far enough to emit an expected serial trace.
5. `git diff --check` reports no whitespace errors.
6. The release notes identify known runtime limitations rather than implying
   unverified hardware support.

## Non-goals

- Claiming production reliability, security certification, or hardware support
  that has not been tested.
- Distributing third-party tool binaries as part of the source repository.
- Treating a successful compile as proof that a driver, filesystem, network
  service, or application is functionally complete.

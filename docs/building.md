# building nv0ken_linux

## required tools

install these on a Debian/Ubuntu host:

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    gcc \
    nasm \
    qemu-system-x86 \
    xorriso \
    mtools \
    git
```

verify versions:

```bash
gcc --version      # need 10+
nasm --version     # need 2.14+
qemu-system-x86_64 --version
xorriso --version
```

## fetch limine

the Limine bootloader binaries must be fetched once before building:

```bash
git clone https://github.com/limine-bootloader/limine.git \
    --branch=v7.x-binary --depth=1 tools/limine
```

## build the kernel

from the project root:

```bash
make
```

this builds `build/kernel.elf`. the top-level Makefile recurses into `kernel/`, `userland/`, `graphics/`, and `apps/`.

to build only the kernel:

```bash
make -C kernel
```

## build the ISO

```bash
bash tools/mkiso.sh
```

output: `build/nv0ken.iso`

## run in QEMU

```bash
bash tools/run_qemu.sh
```

this runs:

```bash
qemu-system-x86_64 \
    -cdrom build/nv0ken.iso \
    -m 256M \
    -cpu qemu64 \
    -serial stdio \
    -vga std \
    -device rtl8139,netdev=net0 \
    -netdev user,id=net0
```

serial output (kprintf) appears in the terminal you ran this from. framebuffer appears in the QEMU window.

## run with KVM acceleration

if you are on a Linux host with KVM available:

```bash
qemu-system-x86_64 \
    -cdrom build/nv0ken.iso \
    -m 256M \
    -cpu host \
    -enable-kvm \
    -serial stdio \
    -vga std \
    -device rtl8139,netdev=net0 \
    -netdev user,id=net0
```

## run with a disk image

to test persistent ext2 storage:

```bash
bash tools/mkfs_ext2.sh
qemu-system-x86_64 \
    -cdrom build/nv0ken.iso \
    -drive file=build/disk.img,format=raw,if=ide \
    -m 256M \
    -serial stdio \
    -vga std
```

## clean build

```bash
make clean
```

removes all `.o` files and binaries under `build/`.

## cross-compilation note

all source files use `-ffreestanding -fno-stack-protector -fno-pie -nostdlib`. do not link against the host libc. the kernel has its own `kernel/lib/` and userspace uses `userland/libc/`. if GCC produces host-specific code, switch to an explicit cross-compiler:

```bash
sudo apt install gcc-x86-64-linux-gnu
```

then set in `toolchain.mk`:

```makefile
CC = x86_64-linux-gnu-gcc
AS = nasm
```
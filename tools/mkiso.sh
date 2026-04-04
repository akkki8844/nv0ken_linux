#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
ISO_ROOT="$ROOT/boot/iso"
LIMINE_DIR="$ISO_ROOT/boot/limine"
EFI_DIR="$ISO_ROOT/EFI/BOOT"
OUT="$BUILD/nv0ken.iso"

if [ ! -f "$ROOT/tools/limine/limine" ]; then
    echo "Limine not found. Fetching..."
    mkdir -p "$ROOT/tools/limine"
    git clone https://github.com/limine-bootloader/limine.git \
        --branch=v7.x-binary --depth=1 "$ROOT/tools/limine"
fi

LIMINE_BIN="$ROOT/tools/limine"

mkdir -p "$LIMINE_DIR"
mkdir -p "$EFI_DIR"

cp "$BUILD/kernel.elf"                    "$ISO_ROOT/kernel.elf"
cp "$ROOT/boot/limine.conf"               "$LIMINE_DIR/limine.conf"
cp "$LIMINE_BIN/limine-bios.sys"          "$LIMINE_DIR/limine-bios.sys"
cp "$LIMINE_BIN/limine-bios-cd.bin"       "$LIMINE_DIR/limine-bios-cd.bin"
cp "$LIMINE_BIN/limine-uefi-cd.bin"       "$LIMINE_DIR/limine-uefi-cd.bin"
cp "$LIMINE_BIN/BOOTX64.EFI"             "$EFI_DIR/BOOTX64.EFI"

xorriso -as mkisofs \
    -b boot/limine/limine-bios-cd.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    --efi-boot boot/limine/limine-uefi-cd.bin \
    -efi-boot-part \
    --efi-boot-image \
    --protective-msdos-label \
    "$ISO_ROOT" \
    -o "$OUT"

"$LIMINE_BIN/limine" bios-install "$OUT"

echo "ISO built: $OUT"
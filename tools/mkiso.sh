#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
ISO_ROOT="$ROOT/boot/iso"
LIMINE_DIR="$ISO_ROOT/boot/limine"
EFI_DIR="$ISO_ROOT/EFI/BOOT"
OUT="$BUILD/nv0ken.iso"

GIT_BIN="${GIT:-git}"
if ! command -v "$GIT_BIN" >/dev/null 2>&1; then
    if [[ -x "/c/Program Files/Git/cmd/git.exe" ]]; then
        GIT_BIN="/c/Program Files/Git/cmd/git.exe"
    else
        echo "error: git is required to fetch Limine" >&2
        exit 1
    fi
fi

if [[ ! -s "$BUILD/kernel.elf" ]]; then
    echo "error: a non-empty kernel image is required; run: make kernel" >&2
    exit 1
fi

if [[ "$(od -An -tx1 -N4 "$BUILD/kernel.elf" | tr -d '[:space:]')" != "7f454c46" ]]; then
    echo "error: kernel image is not an ELF binary: $BUILD/kernel.elf" >&2
    exit 1
fi

if [ ! -f "$ROOT/tools/limine/limine-bios.sys" ] || \
   [ ! -f "$ROOT/tools/limine/limine-bios-cd.bin" ] || \
   [ ! -f "$ROOT/tools/limine/limine-uefi-cd.bin" ] || \
   [ ! -f "$ROOT/tools/limine/BOOTX64.EFI" ] || \
   [ ! -x "$ROOT/tools/limine/limine" ]; then
    if [ -d "$ROOT/tools/limine" ] && [ "$(find "$ROOT/tools/limine" -mindepth 1 -maxdepth 1 | wc -l)" -gt 0 ]; then
        echo "error: Limine directory is incomplete: $ROOT/tools/limine" >&2
        echo "remove it and re-run the build to fetch a complete Limine binary release" >&2
        exit 1
    fi
    echo "Limine not found. Fetching..."
    "$GIT_BIN" clone https://github.com/limine-bootloader/limine.git \
        --branch=v7.x-binary --depth=1 "$ROOT/tools/limine"
fi

LIMINE_BIN="$ROOT/tools/limine"

mkdir -p "$LIMINE_DIR"
mkdir -p "$EFI_DIR"

cp "$BUILD/kernel.elf"                    "$ISO_ROOT/kernel.elf"
rm -f "$ISO_ROOT/limine.cfg" "$LIMINE_DIR/limine.cfg"
# Limine looks for limine.conf on the ISO filesystem.  A .cfg file is not
# discovered automatically and resulted in a boot menu with no entry.
cp "$ROOT/boot/limine.conf"               "$ISO_ROOT/limine.conf"
cp "$ROOT/boot/limine.conf"               "$LIMINE_DIR/limine.conf"
if [ -f "$BUILD/initrd.tar" ]; then
    cp "$BUILD/initrd.tar" "$ISO_ROOT/initrd.tar"
else
    printf '\0\0\0\0' > "$ISO_ROOT/initrd.tar"
fi
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

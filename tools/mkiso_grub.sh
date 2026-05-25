#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
ISO_ROOT="$BUILD/grub_iso"
OUT="$BUILD/nv0ken-grub.iso"

if [[ ! -f "$BUILD/kernel.elf" ]]; then
    echo "error: $BUILD/kernel.elf not found; run: bash build.sh kernel" >&2
    exit 1
fi

if ! command -v grub-mkrescue >/dev/null 2>&1; then
    echo "error: grub-mkrescue not found; install grub-pc-bin grub-common xorriso" >&2
    exit 1
fi

rm -rf "$ISO_ROOT"
mkdir -p "$ISO_ROOT/boot/grub"
cp "$BUILD/kernel.elf" "$ISO_ROOT/boot/kernel.elf"
cp "$ROOT/boot/grub.cfg" "$ISO_ROOT/boot/grub/grub.cfg"

grub-mkrescue -o "$OUT" "$ISO_ROOT"
echo "GRUB ISO built: $OUT"

#!/usr/bin/env bash
set -euo pipefail

required=(
    gcc
    ld
    ar
    nasm
    xorriso
    qemu-system-x86_64
)

optional=(
    grub-mkrescue
    mke2fs
    readelf
    objdump
    nm
    gdb
)

missing=0
for tool in "${required[@]}"; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf "[ ok ] %s -> %s\n" "$tool" "$(command -v "$tool")"
    else
        printf "[miss] %s\n" "$tool"
        missing=1
    fi
done

for tool in "${optional[@]}"; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf "[ opt] %s -> %s\n" "$tool" "$(command -v "$tool")"
    else
        printf "[skip] optional: %s\n" "$tool"
    fi
done

if [[ "$missing" -ne 0 ]]; then
    cat >&2 <<'EOF'

Install the required packages on Debian/Ubuntu:
  sudo apt install build-essential binutils nasm xorriso qemu-system-x86

For GRUB ISO support:
  sudo apt install grub-pc-bin grub-common
EOF
    exit 1
fi

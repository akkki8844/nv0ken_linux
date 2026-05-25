#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ISO="${1:-$ROOT/build/nv0ken.iso}"

if [[ ! -f "$ISO" ]]; then
    echo "error: ISO not found: $ISO" >&2
    echo "run: bash build.sh" >&2
    exit 1
fi

exec qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 512M \
    -cpu qemu64 \
    -serial stdio \
    -vga std \
    -s -S \
    -device rtl8139,netdev=net0 \
    -netdev user,id=net0 \
    ${NV0_QEMU_EXTRA:-}

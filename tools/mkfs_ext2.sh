#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/build/disk.img}"
SIZE_MB="${2:-64}"

mkdir -p "$(dirname "$OUT")"
dd if=/dev/zero of="$OUT" bs=1M count="$SIZE_MB" status=none

if command -v mke2fs >/dev/null 2>&1; then
    mke2fs -q -t ext2 -F "$OUT"
else
    echo "warning: mke2fs not found; created zeroed disk image only" >&2
fi

echo "disk image ready: $OUT (${SIZE_MB}MiB)"

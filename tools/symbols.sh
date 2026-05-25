#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="${1:-$ROOT/build/kernel.elf}"
OUT="${2:-$ROOT/build/kernel.sym}"

if [[ ! -f "$KERNEL" ]]; then
    echo "error: kernel not found: $KERNEL" >&2
    exit 1
fi

nm -n "$KERNEL" > "$OUT"
echo "symbols written: $OUT"

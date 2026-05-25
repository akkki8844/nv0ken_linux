#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="${1:-$ROOT/build/kernel.elf}"

if [[ ! -f "$KERNEL" ]]; then
    echo "error: kernel not found: $KERNEL" >&2
    exit 1
fi

readelf -h "$KERNEL"
readelf -S "$KERNEL"
readelf -l "$KERNEL"

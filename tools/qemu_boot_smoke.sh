#!/usr/bin/env bash
# Boot the GRUB image under QEMU and require the kernel to reach its steady
# state. This catches linker, boot protocol, paging, and early IRQ regressions
# that an ELF-only validation cannot see.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ISO="${1:-$ROOT/build/nv0ken-grub.iso}"
LOG="${2:-$ROOT/build/qemu-serial.log}"
TIMEOUT_SECONDS="${QEMU_SMOKE_TIMEOUT_SECONDS:-12}"

fail() {
    echo "qemu-smoke: $*" >&2
    [[ -f "$LOG" ]] && tail -n 120 "$LOG" >&2
    exit 1
}

command -v qemu-system-x86_64 >/dev/null || fail "qemu-system-x86_64 is required"
[[ -s "$ISO" ]] || fail "missing boot ISO: $ISO"
mkdir -p "$(dirname "$LOG")"
rm -f "$LOG"

set +e
timeout "${TIMEOUT_SECONDS}s" qemu-system-x86_64 \
    -accel tcg \
    -m 256M \
    -cdrom "$ISO" \
    -display none \
    -serial "file:$LOG" \
    -no-reboot \
    -no-shutdown
status=$?
set -e

# The monitor intentionally runs forever, so timeout is the expected result.
[[ "$status" -eq 0 || "$status" -eq 124 ]] || fail "QEMU exited unexpectedly (status $status)"
[[ -s "$LOG" ]] || fail "kernel produced no serial output"
grep -Fq '[ ok ] kernel initialization complete' "$LOG" || fail "kernel did not finish initialization"
grep -Fq 'nv0 monitor ready' "$LOG" || fail "kernel monitor did not become interactive"

echo "qemu-smoke: GRUB boot reached the interactive monitor"

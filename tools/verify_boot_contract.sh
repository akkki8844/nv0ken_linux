#!/usr/bin/env bash
# Validate the artifacts the firmware/bootloader depends on. This stays
# independent of QEMU so CI rejects an unbootable image before publishing it.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
KERNEL="$BUILD/kernel.elf"
INITRD="$BUILD/initrd.tar"

fail() {
    echo "boot-contract: $*" >&2
    exit 1
}

command -v readelf >/dev/null || fail "readelf is required"
command -v tar >/dev/null || fail "tar is required"

[[ -s "$KERNEL" ]] || fail "missing kernel ELF: $KERNEL"
readelf -h "$KERNEL" | grep -Eq 'Class:.*ELF64' || fail "kernel is not ELF64"
readelf -h "$KERNEL" | grep -Eq 'Machine:.*X86-64' || fail "kernel is not x86_64"

entry_hex="$(readelf -h "$KERNEL" | awk '/Entry point address:/ { print $4 }')"
start_hex="$(readelf -sW "$KERNEL" | awk '$8 == "_start" { print "0x" $2; exit }')"
[[ -n "$entry_hex" && -n "$start_hex" ]] || fail "kernel entry symbol is missing"
[[ "$entry_hex" == "$start_hex" ]] || fail "ELF entry $entry_hex does not point to _start ($start_hex)"

for section in .limine_requests_start .limine_requests .limine_requests_end; do
    readelf -SW "$KERNEL" | grep -Fq "$section" || fail "missing Limine section: $section"
done

[[ -s "$INITRD" ]] || fail "missing initrd archive: $INITRD"
for path in ./init ./bin/nv0sh ./etc/inittab; do
    tar -tf "$INITRD" | grep -Fxq "$path" || fail "initrd is missing $path"
done

grep -Fxq 'KERNEL_PATH=boot:///kernel.elf' "$ROOT/boot/limine.conf" || fail "Limine kernel path is invalid"
grep -Fxq 'MODULE_PATH=boot:///initrd.tar' "$ROOT/boot/limine.conf" || fail "Limine initrd module is invalid"

echo "boot-contract: kernel entry, Limine requests, and initrd verified"

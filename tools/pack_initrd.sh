#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
INITRD_ROOT="$BUILD/initrd_root"
OUT="$BUILD/initrd.tar"

rm -rf "$INITRD_ROOT"
mkdir -p \
    "$INITRD_ROOT/bin" \
    "$INITRD_ROOT/apps" \
    "$INITRD_ROOT/etc" \
    "$INITRD_ROOT/fonts" \
    "$INITRD_ROOT/icons" \
    "$INITRD_ROOT/wallpapers" \
    "$INITRD_ROOT/tmp" \
    "$INITRD_ROOT/dev" \
    "$INITRD_ROOT/proc"

copy_if_exists() {
    local src="$1"
    local dst="$2"
    if [[ -e "$src" ]]; then
        mkdir -p "$(dirname "$dst")"
        cp -R "$src" "$dst"
    fi
}

require_file() {
    local path="$1"
    if [[ ! -f "$path" ]]; then
        echo "error: required runtime file is missing: $path" >&2
        exit 1
    fi
}

require_elf_binary() {
    local path="$1"
    require_file "$path"
    if [[ "$(od -An -tx1 -N4 "$path" | tr -d '[:space:]')" != "7f454c46" ]]; then
        echo "error: required runtime file is not an ELF binary: $path" >&2
        exit 1
    fi
}

require_elf_binary "$BUILD/userland/init/init"
require_elf_binary "$BUILD/userland/shell/nv0sh"

for utility in ls cat echo mkdir rm rmdir cp mv pwd touch stat chmod chown \
               ps kill uname date sleep mount umount dmesg env which \
               head tail wc grep find hexdump; do
    require_elf_binary "$BUILD/userland/utils/$utility"
done

for app in terminal text_editor file_manager image_viewer calculator browser; do
    require_elf_binary "$BUILD/apps/$app/$app"
done

copy_if_exists "$ROOT/userland/init/inittab" "$INITRD_ROOT/etc/inittab"
copy_if_exists "$BUILD/userland/init/init" "$INITRD_ROOT/init"
copy_if_exists "$BUILD/userland/shell/nv0sh" "$INITRD_ROOT/bin/nv0sh"

for utility in ls cat echo mkdir rm rmdir cp mv pwd touch stat chmod chown \
               ps kill uname date sleep mount umount dmesg env which \
               head tail wc grep find hexdump; do
    cp "$BUILD/userland/utils/$utility" "$INITRD_ROOT/bin/$utility"
done

for app in terminal text_editor file_manager image_viewer calculator browser; do
    copy_if_exists "$BUILD/apps/$app/$app" "$INITRD_ROOT/apps/$app"
done

copy_if_exists "$ROOT/assets/fonts" "$INITRD_ROOT/fonts"
copy_if_exists "$ROOT/assets/icons" "$INITRD_ROOT/icons"
copy_if_exists "$ROOT/assets/wallpapers" "$INITRD_ROOT/wallpapers"

cat > "$INITRD_ROOT/etc/motd" <<'EOF'
nv0ken_linux
type `help` in nv0sh for shell commands
EOF

tar -C "$INITRD_ROOT" -cf "$OUT" .
echo "initrd packed: $OUT"

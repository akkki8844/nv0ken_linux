#!/bin/bash

set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build"
TOOLS="$ROOT/tools"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
RESET='\033[0m'

log()  { echo -e "${BLUE}[build]${RESET} $*"; }
ok()   { echo -e "${GREEN}[  ok ]${RESET} $*"; }
warn() { echo -e "${YELLOW}[ warn]${RESET} $*"; }
err()  { echo -e "${RED}[error]${RESET} $*" >&2; exit 1; }

usage() {
    cat <<USAGE
usage: build.sh [options] [target]

targets:
  all         build everything and produce ISO (default)
  kernel      build kernel only
  userland    build libc, shell, utils, init
  apps        build all desktop apps
  graphics    build graphics server, WM, desktop
  libnv0      build shared userspace library
  iso         package build/nv0ken.iso (requires kernel built)
  run         build all then launch in QEMU
  run-debug   build all then launch QEMU with GDB stub on :1234
  clean       remove all build artifacts

options:
  -j N        parallel jobs (default: nproc)
  -v          verbose make output
  --no-iso    skip ISO creation after full build
  --kvm       enable KVM acceleration when running
  -h          show this help
USAGE
    exit 0
}

JOBS=$(nproc 2>/dev/null || echo 4)
VERBOSE=0
NO_ISO=0
KVM=0
TARGET="all"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -j)   JOBS="$2"; shift 2 ;;
        -j*)  JOBS="${1#-j}"; shift ;;
        -v)   VERBOSE=1; shift ;;
        --no-iso) NO_ISO=1; shift ;;
        --kvm) KVM=1; shift ;;
        -h|--help) usage ;;
        all|kernel|userland|apps|graphics|libnv0|iso|run|run-debug|clean)
            TARGET="$1"; shift ;;
        *) err "unknown option: $1" ;;
    esac
done

MAKE_FLAGS="-j$JOBS"
if [[ $VERBOSE -eq 1 ]]; then
    MAKE_FLAGS="$MAKE_FLAGS V=1"
fi

check_tools() {
    log "checking toolchain"
    local missing=0
    for tool in gcc nasm qemu-system-x86_64 xorriso; do
        if ! command -v "$tool" &>/dev/null; then
            warn "missing: $tool"
            missing=1
        fi
    done
    if [[ $missing -eq 1 ]]; then
        err "install missing tools with: sudo apt install build-essential nasm qemu-system-x86 xorriso"
    fi
    ok "toolchain ok"
}

fetch_limine() {
    if [[ ! -f "$TOOLS/limine/limine" ]]; then
        log "fetching limine bootloader"
        mkdir -p "$TOOLS/limine"
        git clone https://github.com/limine-bootloader/limine.git \
            --branch=v7.x-binary --depth=1 "$TOOLS/limine" \
            || err "failed to clone limine"
        ok "limine fetched"
    fi
}

build_kernel() {
    log "building kernel"
    make $MAKE_FLAGS -C "$ROOT/kernel" || err "kernel build failed"
    ok "kernel built -> build/kernel.elf"
}

build_libc() {
    log "building libc"
    make $MAKE_FLAGS -C "$ROOT/userland/libc" || err "libc build failed"
    ok "libc built"
}

build_libnv0() {
    log "building libnv0"
    make $MAKE_FLAGS -C "$ROOT/libnv0" || err "libnv0 build failed"
    ok "libnv0 built"
}

build_init() {
    log "building init"
    make $MAKE_FLAGS -C "$ROOT/userland/init" || err "init build failed"
    ok "init built"
}

build_shell() {
    log "building shell"
    make $MAKE_FLAGS -C "$ROOT/userland/shell" || err "shell build failed"
    ok "shell built -> build/userland/shell/nv0sh"
}

build_utils() {
    log "building utils"
    make $MAKE_FLAGS -C "$ROOT/userland/utils" || err "utils build failed"
    ok "utils built"
}

build_graphics() {
    log "building graphics"
    make $MAKE_FLAGS -C "$ROOT/graphics" || err "graphics build failed"
    ok "graphics built"
}

build_apps() {
    log "building apps"
    for app in terminal text_editor file_manager image_viewer calculator browser; do
        if [[ -f "$ROOT/apps/$app/Makefile" ]]; then
            log "  -> $app"
            make $MAKE_FLAGS -C "$ROOT/apps/$app" || err "$app build failed"
        fi
    done
    ok "apps built"
}

build_initrd() {
    log "packing initrd"
    bash "$TOOLS/pack_initrd.sh" || err "initrd packing failed"
    ok "initrd packed -> build/initrd.tar"
}

build_iso() {
    log "building ISO"
    fetch_limine
    bash "$TOOLS/mkiso.sh" || err "ISO build failed"
    ok "ISO built -> build/nv0ken.iso"
}

run_qemu() {
    local debug="$1"
    if [[ ! -f "$BUILD/nv0ken.iso" ]]; then
        err "build/nv0ken.iso not found — run build.sh first"
    fi

    local qemu_flags=(
        -cdrom "$BUILD/nv0ken.iso"
        -m 256M
        -cpu qemu64
        -serial stdio
        -vga std
        -device rtl8139,netdev=net0
        -netdev user,id=net0
    )

    if [[ $KVM -eq 1 ]]; then
        qemu_flags+=(-enable-kvm -cpu host)
        log "KVM enabled"
    fi

    if [[ -f "$BUILD/disk.img" ]]; then
        qemu_flags+=(-drive "file=$BUILD/disk.img,format=raw,if=ide")
        log "attaching disk.img"
    fi

    if [[ "$debug" == "debug" ]]; then
        qemu_flags+=(-s -S)
        log "GDB stub on :1234 — waiting for debugger"
        log "run: gdb -x tools/gdbinit"
    fi

    log "launching QEMU"
    qemu-system-x86_64 "${qemu_flags[@]}"
}

mkdir -p "$BUILD"

case "$TARGET" in
    clean)
        log "cleaning build artifacts"
        rm -rf "$BUILD"
        ok "clean done"
        ;;

    kernel)
        check_tools
        build_kernel
        ;;

    userland)
        check_tools
        build_libc
        build_init
        build_shell
        build_utils
        ;;

    libnv0)
        check_tools
        build_libc
        build_libnv0
        ;;

    graphics)
        check_tools
        build_libc
        build_libnv0
        build_graphics
        ;;

    apps)
        check_tools
        build_libc
        build_libnv0
        build_apps
        ;;

    iso)
        build_iso
        ;;

    run)
        check_tools
        build_kernel
        build_libc
        build_libnv0
        build_init
        build_shell
        build_utils
        build_graphics
        build_apps
        build_initrd
        build_iso
        run_qemu ""
        ;;

    run-debug)
        check_tools
        build_kernel
        build_libc
        build_libnv0
        build_init
        build_shell
        build_utils
        build_graphics
        build_apps
        build_initrd
        build_iso
        run_qemu "debug"
        ;;

    all)
        check_tools
        build_kernel
        build_libc
        build_libnv0
        build_init
        build_shell
        build_utils
        build_graphics
        build_apps
        build_initrd
        if [[ $NO_ISO -eq 0 ]]; then
            build_iso
        fi
        echo ""
        ok "build complete"
        if [[ $NO_ISO -eq 0 ]]; then
            echo -e "  ${GREEN}ISO:${RESET} build/nv0ken.iso"
            echo -e "  run with: ${YELLOW}bash build.sh run${RESET}"
        fi
        ;;

    *)
        err "unknown target: $TARGET"
        ;;
esac
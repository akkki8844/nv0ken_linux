include toolchain.mk

ROOT    := $(shell pwd)
BUILD   := $(ROOT)/build

SUBMAKES := \
    kernel \
    userland/libc \
    libnv0 \
    userland/init \
    userland/shell \
    userland/utils \
    graphics \
    apps/terminal \
    apps/text_editor \
    apps/file_manager \
    apps/image_viewer \
    apps/calculator \
    apps/browser

.PHONY: all clean $(SUBMAKES) iso grub-iso initrd run run-grub run-debug

all: $(SUBMAKES) initrd iso

kernel:
	$(MAKE) -C kernel

userland/libc:
	$(MAKE) -C userland/libc

libnv0: userland/libc
	$(MAKE) -C libnv0

userland/init: userland/libc
	$(MAKE) -C userland/init

userland/shell: userland/libc
	$(MAKE) -C userland/shell

userland/utils: userland/libc
	$(MAKE) -C userland/utils

graphics: userland/libc libnv0
	$(MAKE) -C graphics

apps/terminal: userland/libc libnv0
	$(MAKE) -C apps/terminal

apps/text_editor: userland/libc libnv0
	$(MAKE) -C apps/text_editor

apps/file_manager: userland/libc libnv0
	$(MAKE) -C apps/file_manager

apps/image_viewer: userland/libc libnv0
	$(MAKE) -C apps/image_viewer

apps/calculator: userland/libc libnv0
	$(MAKE) -C apps/calculator

apps/browser: userland/libc libnv0
	$(MAKE) -C apps/browser

initrd:
	bash tools/pack_initrd.sh

iso: kernel
	bash tools/mkiso.sh

grub-iso: kernel initrd
	bash tools/mkiso_grub.sh

run: all
	bash tools/run_qemu.sh

run-grub: grub-iso
	bash tools/run_qemu.sh build/nv0ken-grub.iso

run-debug: all
	bash tools/run_qemu_debug.sh

clean:
	rm -rf $(BUILD)

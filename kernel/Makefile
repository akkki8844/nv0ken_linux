include ../toolchain.mk

ROOT  := ..
BUILD := $(ROOT)/build
OUT   := $(BUILD)/kernel

CFLAGS  := $(FREESTANDING_CFLAGS) -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 -msoft-float -I. -I$(ROOT)/boot
ASFLAGS := -f elf64
LDFLAGS := -nostdlib -static -z max-page-size=0x1000

SRC_C := $(shell find . -name '*.c')
SRC_ASM := $(shell find . -name '*.asm')
BOOT_ASM := ../boot/multiboot2_header.asm

OBJ_C := $(patsubst ./%.c,$(OUT)/%.c.o,$(SRC_C))
COMMON_ASM := $(filter-out ./arch/x86_64/boot_entry.asm ./arch/x86_64/limine_entry.asm,$(SRC_ASM))
OBJ_COMMON_ASM := $(patsubst ./%.asm,$(OUT)/%.asm.o,$(COMMON_ASM))
OBJ_GRUB_ASM := $(OUT)/arch/x86_64/boot_entry.asm.o
OBJ_LIMINE_ASM := $(OUT)/arch/x86_64/limine_entry.asm.o
OBJ_BOOT_ASM := $(OUT)/boot/multiboot2_header.asm.o
GRUB_OBJS := $(OBJ_BOOT_ASM) $(OBJ_GRUB_ASM) $(OBJ_COMMON_ASM) $(OBJ_C)
LIMINE_OBJS := $(OBJ_LIMINE_ASM) $(OBJ_COMMON_ASM) $(OBJ_C)

LIMINE_TARGET := $(BUILD)/kernel.elf
GRUB_TARGET := $(BUILD)/kernel-grub.elf

.PHONY: all clean FORCE

all: $(LIMINE_TARGET) $(GRUB_TARGET)

$(LIMINE_TARGET): FORCE $(LIMINE_OBJS) kernel_limine.ld
	$(LD) $(LDFLAGS) -T kernel_limine.ld $(LIMINE_OBJS) -o $@

$(GRUB_TARGET): FORCE $(GRUB_OBJS) kernel.ld
	$(LD) $(LDFLAGS) -T kernel.ld $(GRUB_OBJS) -o $@

FORCE:

$(OUT)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/%.asm.o: %.asm
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ_BOOT_ASM): $(BOOT_ASM)
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf $(OUT) $(LIMINE_TARGET) $(GRUB_TARGET)

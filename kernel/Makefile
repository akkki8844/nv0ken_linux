include ../toolchain.mk

ROOT  := ..
BUILD := $(ROOT)/build
OUT   := $(BUILD)/kernel

CFLAGS  := $(FREESTANDING_CFLAGS) -mcmodel=kernel -I. -I$(ROOT)/boot
ASFLAGS := -f elf64
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -T kernel.ld

SRC_C := $(shell find . -name '*.c')
SRC_ASM := $(shell find . -name '*.asm')
BOOT_ASM := ../boot/multiboot2_header.asm

OBJ_C := $(patsubst ./%.c,$(OUT)/%.c.o,$(SRC_C))
OBJ_ASM := $(patsubst ./%.asm,$(OUT)/%.asm.o,$(SRC_ASM))
OBJ_BOOT_ASM := $(OUT)/boot/multiboot2_header.asm.o
OBJS := $(OBJ_BOOT_ASM) $(OBJ_ASM) $(OBJ_C)

TARGET := $(BUILD)/kernel.elf

.PHONY: all clean FORCE

all: $(TARGET)

$(TARGET): FORCE $(OBJS) kernel.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

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
	rm -rf $(OUT) $(TARGET)

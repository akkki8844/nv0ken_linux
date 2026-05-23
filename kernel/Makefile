include ../toolchain.mk

ROOT  := ..
BUILD := $(ROOT)/build
OUT   := $(BUILD)/kernel

CFLAGS  := $(FREESTANDING_CFLAGS) -mcmodel=kernel -I. -I$(ROOT)/boot
ASFLAGS := -f elf64
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -T kernel.ld

SRC_C := $(shell find . -name '*.c')
SRC_ASM := $(shell find . -name '*.asm')

OBJ_C := $(patsubst ./%.c,$(OUT)/%.o,$(SRC_C))
OBJ_ASM := $(patsubst ./%.asm,$(OUT)/%.o,$(SRC_ASM))
OBJS := $(OBJ_ASM) $(OBJ_C)

TARGET := $(BUILD)/kernel.elf

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) kernel.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(OUT)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/%.o: %.asm
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf $(OUT) $(TARGET)

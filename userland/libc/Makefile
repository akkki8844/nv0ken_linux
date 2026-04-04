CC     = gcc
AS     = nasm

CFLAGS = -std=c11 -Wall -Wextra \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g \
         -Iinclude

ASFLAGS = -f elf64

ROOT  = ../..
BUILD = $(ROOT)/build/userland/libc

SRC_C = \
    src/errno.c \
    src/string.c \
    src/ctype.c \
    src/stdlib.c \
    src/stdio.c \
    src/unistd.c \
    src/signal.c \
    src/assert.c

SRC_ASM = \
    src/crt0.asm \
    src/syscall.asm

OBJ_C   = $(SRC_C:src/%.c=$(BUILD)/%.o)
OBJ_ASM = $(SRC_ASM:src/%.asm=$(BUILD)/%.o)
OBJS    = $(OBJ_C) $(OBJ_ASM)

TARGET  = $(BUILD)/libc.a
CRT0    = $(BUILD)/crt0.o

.PHONY: all clean

all: $(BUILD) $(TARGET) $(CRT0)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(OBJS)
	ar rcs $@ $(filter-out $(BUILD)/crt0.o, $(OBJS))

$(CRT0): $(BUILD)/crt0.o

$(BUILD)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.asm
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf $(BUILD)

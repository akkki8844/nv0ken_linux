CC     = gcc
LD     = ld

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g

ROOT   = ../..
LIBNV0 = $(ROOT)/libnv0
LIBC   = $(ROOT)/userland/libc
BUILD  = $(ROOT)/build/apps/image_viewer
CRT0   = $(ROOT)/build/userland/libc/crt0.o

INCLUDES = \
    -I. \
    -I$(LIBNV0)/include \
    -I$(LIBC)/include

LDFLAGS = \
    -L$(ROOT)/build/libnv0 \
    -L$(ROOT)/build/userland/libc

LDLIBS = --start-group -lnv0 -lc --end-group

TARGET = $(BUILD)/image_viewer

SRCS = \
    main.c \
    decode_bmp.c \
    decode_ppm.c

OBJS = $(SRCS:%.c=$(BUILD)/%.o)

.PHONY: all clean

all: $(BUILD) $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(OBJS) $(CRT0)
	$(LD) -m elf_x86_64 -static -o $@ $(CRT0) $(OBJS) $(LDFLAGS) $(LDLIBS)

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD)

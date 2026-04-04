CC      = gcc
AS      = nasm
AR      = ar
OBJCOPY = objcopy

CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic \
          -ffreestanding -fno-stack-protector \
          -fno-pie -fno-pic -m64 \
          -O2 -g

ASFLAGS = -f elf64

ROOT    = ../../..
LIBNV0  = $(ROOT)/libnv0
LIBC    = $(ROOT)/userland/libc
BUILD   = $(ROOT)/build/apps/browser

INCLUDES = \
    -I. \
    -I$(LIBNV0)/include \
    -I$(LIBC)/include

LDFLAGS = \
    -L$(ROOT)/build/libnv0 \
    -L$(ROOT)/build/userland/libc \
    -lnv0 \
    -lc \
    -nostdlib \
    -static

TARGET  = $(BUILD)/browser

SRCS = \
    main.c \
    http_client.c \
    html_parser.c \
    renderer.c

OBJS = $(SRCS:%.c=$(BUILD)/%.o)

.PHONY: all clean

all: $(BUILD) $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD)
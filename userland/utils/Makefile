CC     = gcc

CFLAGS = -std=c11 -Wall -Wextra \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g

ROOT   = ../..
LIBC   = $(ROOT)/userland/libc
BUILD  = $(ROOT)/build/userland/utils

INCLUDES = -I$(LIBC)/include
CRT0     = $(ROOT)/build/userland/libc/crt0.o

LDFLAGS = \
    -L$(ROOT)/build/userland/libc \
    -lc \
    -nostdlib \
    -static

UTILS = ls cat echo mkdir rm rmdir cp mv pwd touch stat chmod chown \
        ps kill uname date sleep mount umount dmesg env which \
        head tail wc grep find hexdump

TARGETS = $(addprefix $(BUILD)/, $(UTILS))

.PHONY: all clean

all: $(BUILD) $(TARGETS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(CRT0) $< $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD)
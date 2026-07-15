CC     = gcc
LD     = ld

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
    -L$(ROOT)/build/userland/libc

LDLIBS = --start-group -lc --end-group

UTILS = ls cat echo mkdir rm rmdir cp mv pwd touch stat chmod chown \
        ps kill uname date sleep mount umount dmesg env which \
        head tail wc grep find hexdump

TARGETS = $(addprefix $(BUILD)/, $(UTILS))
OBJS    = $(addsuffix .o, $(TARGETS))

.PHONY: all clean

all: $(BUILD) $(TARGETS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%: $(BUILD)/%.o $(CRT0)
	$(LD) -m elf_x86_64 -static -o $@ $(CRT0) $< $(LDFLAGS) $(LDLIBS)

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD)

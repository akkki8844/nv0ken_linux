CC     = gcc
LD     = ld

CFLAGS = -std=c11 -Wall -Wextra \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g

ROOT   = ../..
LIBC   = $(ROOT)/userland/libc
BUILD  = $(ROOT)/build/userland/init
CRT0   = $(ROOT)/build/userland/libc/crt0.o

INCLUDES = -I$(LIBC)/include

LDFLAGS = \
    -L$(ROOT)/build/userland/libc

LDLIBS = --start-group -lc --end-group

TARGET = $(BUILD)/init

SRCS   = init.c
OBJS   = $(SRCS:%.c=$(BUILD)/%.o)

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

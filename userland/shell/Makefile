CC     = gcc
LD     = ld

CFLAGS = -std=c11 -Wall -Wextra \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g

ROOT   = ../..
LIBC   = $(ROOT)/userland/libc
BUILD  = $(ROOT)/build/userland/shell

INCLUDES = -I. -I$(LIBC)/include

LDFLAGS = \
    -L$(ROOT)/build/userland/libc

LDLIBS = --start-group -lc --end-group

TARGET = $(BUILD)/nv0sh

SRCS = \
    main.c \
    parser.c \
    builtins.c \
    job_control.c \
    readline.c \
    expand.c

OBJS = $(SRCS:%.c=$(BUILD)/%.o)
CRT0 = $(ROOT)/build/userland/libc/crt0.o

.PHONY: all clean

all: $(BUILD) $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(OBJS)
	$(LD) -m elf_x86_64 -static -o $@ $(CRT0) $(OBJS) $(LDFLAGS) $(LDLIBS)

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD)

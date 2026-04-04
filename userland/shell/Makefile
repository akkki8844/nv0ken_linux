CC     = gcc

CFLAGS = -std=c11 -Wall -Wextra \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g

ROOT   = ../..
LIBC   = $(ROOT)/userland/libc
BUILD  = $(ROOT)/build/userland/shell

INCLUDES = -I. -I$(LIBC)/include

LDFLAGS = \
    -L$(ROOT)/build/userland/libc \
    -lc \
    -nostdlib \
    -static

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
	$(CC) $(CRT0) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD)

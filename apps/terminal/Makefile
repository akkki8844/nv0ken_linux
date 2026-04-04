CC     = gcc

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g

ROOT   = ../../..
LIBNV0 = $(ROOT)/libnv0
LIBC   = $(ROOT)/userland/libc
BUILD  = $(ROOT)/build/apps/terminal

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

TARGET = $(BUILD)/terminal

SRCS = \
    main.c \
    vt100.c \
    pty.c

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
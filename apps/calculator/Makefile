CC     = gcc
AR     = ar

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g

ROOT   = ../../..
LIBNV0 = $(ROOT)/libnv0
LIBC   = $(ROOT)/userland/libc
BUILD  = $(ROOT)/build/apps/calculator

INCLUDES = \
    -I. \
    -I$(LIBNV0)/include \
    -I$(LIBC)/include

LDFLAGS = \
    -L$(ROOT)/build/libnv0 \
    -L$(ROOT)/build/userland/libc \
    -lnv0 \
    -lc \
    -lm \
    -nostdlib \
    -static

TARGET = $(BUILD)/calculator

SRCS = main.c

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
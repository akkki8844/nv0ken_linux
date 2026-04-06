CC     = gcc

CFLAGS = -std=c11 -Wall -Wextra \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g

ROOT     = ../../..
LIBC     = $(ROOT)/userland/libc
BUILD    = $(ROOT)/build/graphics/wm

INCLUDES = \
    -I. \
    -I.. \
    -I$(LIBC)/include

SRCS = \
    wm.c \
    window.c \
    titlebar.c \
    resize.c \
    layout.c

OBJS = $(SRCS:%.c=$(BUILD)/%.o)
LIB  = $(BUILD)/libwm.a

.PHONY: all clean

all: $(BUILD) $(LIB)

$(BUILD):
	mkdir -p $(BUILD)

$(LIB): $(OBJS)
	ar rcs $@ $(OBJS)

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD)

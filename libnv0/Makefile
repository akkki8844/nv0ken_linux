CC     ?= gcc
AR     ?= ar

ROOT   = ..
BUILD  = $(ROOT)/build/libnv0

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g \
         -Iinclude \
         -I$(ROOT)/userland/libc/include \
         -I$(ROOT)/graphics \
         -I$(ROOT)

SRCS = \
    src/app.c \
    src/dialog.c \
    src/draw.c \
    src/fs.c \
    src/input.c \
    src/ipc.c \
    src/window.c \
    $(ROOT)/graphics/draw/draw.c \
    $(ROOT)/graphics/font/psf.c \
    $(ROOT)/graphics/font/font_render.c

OBJS = $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
TARGET = $(BUILD)/libnv0.a

.PHONY: all clean

all: $(BUILD) $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $(OBJS)

$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

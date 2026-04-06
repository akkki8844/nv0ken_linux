CC     = gcc

CFLAGS = -std=c11 -Wall -Wextra \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g

ROOT   = ../../..
LIBC   = $(ROOT)/userland/libc
BUILD  = $(ROOT)/build/graphics/font

INCLUDES = \
    -I. \
    -I.. \
    -I$(LIBC)/include

SRCS = \
    psf.c \
    font_render.c

OBJS = $(SRCS:%.c=$(BUILD)/%.o)
LIB  = $(BUILD)/libfont.a

.PHONY: all clean

all: $(BUILD) $(LIB)

$(BUILD):
	mkdir -p $(BUILD)

$(LIB): $(OBJS)
	ar rcs $@ $(OBJS)

$(BUILD)/psf.o:        psf.c        psf.h
$(BUILD)/font_render.o: font_render.c font_render.h psf.h ../draw/draw.h

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD)

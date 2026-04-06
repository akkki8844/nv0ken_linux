CC     = gcc

CFLAGS = -std=c11 -Wall -Wextra \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g

ROOT   = ../../..
LIBC   = $(ROOT)/userland/libc
BUILD  = $(ROOT)/build/graphics/draw

INCLUDES = \
    -I. \
    -I$(LIBC)/include

SRCS = \
    draw.c \
    image.c \
    clip.c

OBJS = $(SRCS:%.c=$(BUILD)/%.o)
LIB  = $(BUILD)/libdraw.a

.PHONY: all clean

all: $(BUILD) $(LIB)

$(BUILD):
	mkdir -p $(BUILD)

$(LIB): $(OBJS)
	ar rcs $@ $(OBJS)

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/draw.o:  draw.c  draw.h  color.h
$(BUILD)/image.o: image.c image.h color.h
$(BUILD)/clip.o:  clip.c  clip.h  draw.h

clean:
	rm -rf $(BUILD)

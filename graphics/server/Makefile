CC     = gcc

CFLAGS = -std=c11 -Wall -Wextra \
         -ffreestanding -fno-stack-protector \
         -fno-pie -fno-pic -m64 \
         -O2 -g

ROOT     = ../../..
LIBC     = $(ROOT)/userland/libc
BUILD    = $(ROOT)/build/graphics/server

INCLUDES = \
    -I. \
    -I.. \
    -I$(LIBC)/include

SRCS = \
    compositor.c \
    surface.c \
    damage.c \
    display.c \
    ipc_server.c \
    event_loop.c \
    cursor.c

OBJS = $(SRCS:%.c=$(BUILD)/%.o)
LIB  = $(BUILD)/libserver.a

.PHONY: all clean

all: $(BUILD) $(LIB)

$(BUILD):
	mkdir -p $(BUILD)

$(LIB): $(OBJS)
	ar rcs $@ $(OBJS)

$(BUILD)/compositor.o: compositor.c compositor.h ../draw/draw.h ../draw/color.h
$(BUILD)/surface.o:    surface.c    surface.h    compositor.h ../draw/color.h
$(BUILD)/damage.o:     damage.c     damage.h     ../draw/draw.h
$(BUILD)/display.o:    display.c    display.h    compositor.h ../draw/draw.h
$(BUILD)/ipc_server.o: ipc_server.c ipc_server.h compositor.h
$(BUILD)/event_loop.o: event_loop.c event_loop.h display.h ipc_server.h cursor.h
$(BUILD)/cursor.o:     cursor.c     cursor.h     compositor.h \
                       ../draw/draw.h ../draw/color.h ../draw/image.h

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD)

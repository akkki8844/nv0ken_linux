CC      := gcc
LD      := ld
AS      := nasm
AR      := ar
OBJCOPY := objcopy

HOSTCC  := gcc

COMMON_WARNINGS := -Wall -Wextra -Wpedantic
FREESTANDING_CFLAGS := -std=c11 $(COMMON_WARNINGS) \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
    -fno-builtin -m64 -mno-red-zone -O2 -g

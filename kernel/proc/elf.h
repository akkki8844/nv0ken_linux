#ifndef NV0KEN_PROC_ELF_H
#define NV0KEN_PROC_ELF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ELF_MAGIC 0x464C457F
#define ELF_CLASS_64 2
#define ELF_DATA_LSB 1
#define ELF_MACHINE_X86_64 0x3E
#define ELF_PT_LOAD 1

typedef struct {
    unsigned char ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf64_header_t;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} elf64_program_header_t;

bool elf64_validate(const void *image, size_t size);

#endif

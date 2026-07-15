#include "elf_loader.h"

#include "../lib/string.h"
#include "elf.h"

bool elf64_validate(const void *image, size_t size)
{
    if (!image || size < sizeof(elf64_header_t)) {
        return false;
    }

    const elf64_header_t *header = image;
    uint32_t magic = 0;
    memcpy(&magic, header->ident, sizeof(magic));
    if (magic != ELF_MAGIC ||
        header->ident[4] != ELF_CLASS_64 ||
        header->ident[5] != ELF_DATA_LSB ||
        header->machine != ELF_MACHINE_X86_64 ||
        header->ehsize != sizeof(elf64_header_t) ||
        header->phentsize != sizeof(elf64_program_header_t) ||
        header->phnum == 0 || header->phoff > size) {
        return false;
    }

    return header->phnum <= (size - (size_t)header->phoff) / header->phentsize;
}

int elf_load_process(process_t *process, const void *image, size_t size, elf_load_result_t *result)
{
    if (!process || !elf64_validate(image, size)) {
        return -1;
    }

    const elf64_header_t *header = image;
    uintptr_t low = UINTPTR_MAX;
    uintptr_t high = 0;

    for (uint16_t index = 0; index < header->phnum; ++index) {
        size_t offset = header->phoff + (size_t)index * header->phentsize;
        if (offset + sizeof(elf64_program_header_t) > size) {
            return -1;
        }

        const elf64_program_header_t *program = (const void *)((const unsigned char *)image + offset);
        if (program->type != ELF_PT_LOAD) {
            continue;
        }
        if (program->offset > size || program->filesz > size - program->offset ||
            program->filesz > program->memsz ||
            program->vaddr + program->memsz < program->vaddr) {
            return -1;
        }

        if (program->memsz == 0) {
            continue;
        }

        if ((uintptr_t)program->vaddr < low) {
            low = (uintptr_t)program->vaddr;
        }
        if ((uintptr_t)(program->vaddr + program->memsz) > high) {
            high = (uintptr_t)(program->vaddr + program->memsz);
        }
    }

    if (low == UINTPTR_MAX || (uintptr_t)header->entry < low ||
        (uintptr_t)header->entry >= high) {
        return -1;
    }

    if (result) {
        result->entry = (uintptr_t)header->entry;
        result->lowest_vaddr = low == UINTPTR_MAX ? 0 : low;
        result->highest_vaddr = high;
    }
    return 0;
}

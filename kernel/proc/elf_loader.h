#ifndef NV0KEN_PROC_ELF_LOADER_H
#define NV0KEN_PROC_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

#include "process.h"

typedef struct {
    uintptr_t entry;
    uintptr_t lowest_vaddr;
    uintptr_t highest_vaddr;
} elf_load_result_t;

int elf_load_process(process_t *process, const void *image, size_t size, elf_load_result_t *result);

#endif

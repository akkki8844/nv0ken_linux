#ifndef NV0KEN_MM_ADDR_SPACE_H
#define NV0KEN_MM_ADDR_SPACE_H

#include <stdint.h>
#include "vmm.h"

typedef struct addr_space {
    vmm_space_t vmm;
    uint64_t heap_base;
    uint64_t heap_end;
    uint64_t stack_top;
    int refcount;
} addr_space_t;

void addr_space_init_kernel(addr_space_t *space, uint64_t pml4_phys, uint64_t hhdm_offset);
addr_space_t *addr_space_current(void);
addr_space_t *addr_space_kernel(void);
void addr_space_switch(addr_space_t *space);
void addr_space_retain(addr_space_t *space);
void addr_space_release(addr_space_t *space);

#endif

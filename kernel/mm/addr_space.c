#include "addr_space.h"

static addr_space_t *current_space;

void addr_space_init_kernel(addr_space_t *space, uint64_t pml4_phys, uint64_t hhdm_offset)
{
    if (!space) {
        return;
    }
    space->vmm.pml4_phys = pml4_phys;
    space->vmm.hhdm_offset = hhdm_offset;
    space->heap_base = 0;
    space->heap_end = 0;
    space->stack_top = 0;
    space->refcount = 1;
    current_space = space;
}

addr_space_t *addr_space_current(void)
{
    return current_space;
}

addr_space_t *addr_space_kernel(void)
{
    return current_space;
}

void addr_space_switch(addr_space_t *space)
{
    if (!space) {
        return;
    }
    current_space = space;
    vmm_activate(&space->vmm);
}

void addr_space_retain(addr_space_t *space)
{
    if (space) {
        ++space->refcount;
    }
}

void addr_space_release(addr_space_t *space)
{
    if (space && space->refcount > 0) {
        --space->refcount;
    }
}

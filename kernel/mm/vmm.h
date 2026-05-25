#ifndef NV0KEN_MM_VMM_H
#define NV0KEN_MM_VMM_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t pml4_phys;
    uint64_t hhdm_offset;
} vmm_space_t;

void vmm_init(uint64_t kernel_pml4, uint64_t hhdm_offset);
vmm_space_t *vmm_kernel_space(void);
int vmm_map_page(vmm_space_t *space, uint64_t virt, uint64_t phys, uint64_t flags);
int vmm_unmap_page(vmm_space_t *space, uint64_t virt);
uint64_t vmm_virt_to_phys(vmm_space_t *space, uint64_t virt);
void vmm_activate(vmm_space_t *space);

#endif

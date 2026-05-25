#include "vmm.h"

#include "arch/x86_64/paging.h"

static vmm_space_t kernel_space;

void vmm_init(uint64_t kernel_pml4, uint64_t hhdm_offset)
{
    kernel_space.pml4_phys = kernel_pml4 ? kernel_pml4 : paging_current_cr3();
    kernel_space.hhdm_offset = hhdm_offset;
}

vmm_space_t *vmm_kernel_space(void)
{
    return &kernel_space;
}

static pte_t *phys_to_virt(vmm_space_t *space, uint64_t phys)
{
    return (pte_t *)(uintptr_t)(phys + (space ? space->hhdm_offset : 0));
}

int vmm_map_page(vmm_space_t *space, uint64_t virt, uint64_t phys, uint64_t flags)
{
    if (!space || !space->pml4_phys) {
        return -1;
    }

    pte_t *pml4 = phys_to_virt(space, space->pml4_phys);
    pte_t *pdpt = phys_to_virt(space, pml4[pml4_index(virt)] & PAGE_MASK);
    if (!pdpt) {
        return -1;
    }
    pte_t *pd = phys_to_virt(space, pdpt[pdpt_index(virt)] & PAGE_MASK);
    if (!pd) {
        return -1;
    }
    pte_t *pt = phys_to_virt(space, pd[pd_index(virt)] & PAGE_MASK);
    if (!pt) {
        return -1;
    }
    pt[pt_index(virt)] = (phys & PAGE_MASK) | flags | PTE_PRESENT;
    paging_invalidate_page(virt);
    return 0;
}

int vmm_unmap_page(vmm_space_t *space, uint64_t virt)
{
    if (!space || !space->pml4_phys) {
        return -1;
    }
    pte_t *pml4 = phys_to_virt(space, space->pml4_phys);
    pte_t *pdpt = phys_to_virt(space, pml4[pml4_index(virt)] & PAGE_MASK);
    pte_t *pd = phys_to_virt(space, pdpt[pdpt_index(virt)] & PAGE_MASK);
    pte_t *pt = phys_to_virt(space, pd[pd_index(virt)] & PAGE_MASK);
    pt[pt_index(virt)] = 0;
    paging_invalidate_page(virt);
    return 0;
}

uint64_t vmm_virt_to_phys(vmm_space_t *space, uint64_t virt)
{
    if (!space || !space->pml4_phys) {
        return 0;
    }
    pte_t *pml4 = phys_to_virt(space, space->pml4_phys);
    pte_t pml4e = pml4[pml4_index(virt)];
    if (!(pml4e & PTE_PRESENT)) {
        return 0;
    }
    pte_t *pdpt = phys_to_virt(space, pml4e & PAGE_MASK);
    pte_t pdpte = pdpt[pdpt_index(virt)];
    if (!(pdpte & PTE_PRESENT)) {
        return 0;
    }
    pte_t *pd = phys_to_virt(space, pdpte & PAGE_MASK);
    pte_t pde = pd[pd_index(virt)];
    if (!(pde & PTE_PRESENT)) {
        return 0;
    }
    if (pde & PTE_HUGE) {
        return (pde & ~((1ull << 21) - 1)) | (virt & ((1ull << 21) - 1));
    }
    pte_t *pt = phys_to_virt(space, pde & PAGE_MASK);
    pte_t pte = pt[pt_index(virt)];
    if (!(pte & PTE_PRESENT)) {
        return 0;
    }
    return (pte & PAGE_MASK) | (virt & (PAGE_SIZE - 1));
}

void vmm_activate(vmm_space_t *space)
{
    if (space && space->pml4_phys) {
        paging_load_cr3(space->pml4_phys);
    }
}

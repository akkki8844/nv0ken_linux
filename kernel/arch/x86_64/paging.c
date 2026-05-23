#include "paging.h"

#include "cpu.h"

static uint64_t current_pml4;

void paging_init(uint64_t pml4_phys)
{
    current_pml4 = pml4_phys;
    if (pml4_phys) {
        paging_load_cr3(pml4_phys);
    } else {
        current_pml4 = cpu_read_cr3();
    }
}

void paging_load_cr3(uint64_t pml4_phys)
{
    current_pml4 = pml4_phys;
    cpu_write_cr3(pml4_phys);
}

uint64_t paging_current_cr3(void)
{
    return current_pml4 ? current_pml4 : cpu_read_cr3();
}

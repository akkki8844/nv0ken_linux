#ifndef NV0KEN_ARCH_X86_64_PAGING_H
#define NV0KEN_ARCH_X86_64_PAGING_H

#include <stdint.h>

#define PAGE_SIZE 4096ull
#define PAGE_MASK (~(PAGE_SIZE - 1ull))

#define PTE_PRESENT  (1ull << 0)
#define PTE_WRITABLE (1ull << 1)
#define PTE_USER     (1ull << 2)
#define PTE_WRITETHROUGH (1ull << 3)
#define PTE_NOCACHE  (1ull << 4)
#define PTE_ACCESSED (1ull << 5)
#define PTE_DIRTY    (1ull << 6)
#define PTE_HUGE     (1ull << 7)
#define PTE_GLOBAL   (1ull << 8)
#define PTE_NOEXEC   (1ull << 63)

typedef uint64_t pte_t;

static inline uint64_t page_align_down(uint64_t value)
{
    return value & PAGE_MASK;
}

static inline uint64_t page_align_up(uint64_t value)
{
    return (value + PAGE_SIZE - 1) & PAGE_MASK;
}

static inline uint16_t pml4_index(uint64_t virt)
{
    return (uint16_t)((virt >> 39) & 0x1ff);
}

static inline uint16_t pdpt_index(uint64_t virt)
{
    return (uint16_t)((virt >> 30) & 0x1ff);
}

static inline uint16_t pd_index(uint64_t virt)
{
    return (uint16_t)((virt >> 21) & 0x1ff);
}

static inline uint16_t pt_index(uint64_t virt)
{
    return (uint16_t)((virt >> 12) & 0x1ff);
}

void paging_init(uint64_t pml4_phys);
void paging_load_cr3(uint64_t pml4_phys);
void paging_invalidate_page(uint64_t virt);
uint64_t paging_current_cr3(void);

#endif

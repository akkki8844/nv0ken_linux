#ifndef NV0KEN_MM_COW_H
#define NV0KEN_MM_COW_H

#include <stddef.h>
#include <stdint.h>

#define COW_FLAG  (1ull << 9)

void cow_init(void);
int cow_mark_page(uint64_t virt);
int cow_handle_fault(uint64_t fault_addr, uint64_t error_code);
size_t cow_fault_count(void);

#endif

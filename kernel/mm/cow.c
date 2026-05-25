#include "cow.h"

#include <stddef.h>

static size_t handled_faults;

void cow_init(void)
{
    handled_faults = 0;
}

int cow_mark_page(uint64_t virt)
{
    (void)virt;
    return 0;
}

int cow_handle_fault(uint64_t fault_addr, uint64_t error_code)
{
    (void)fault_addr;
    if ((error_code & 0x2) == 0) {
        return -1;
    }
    ++handled_faults;
    return 0;
}

size_t cow_fault_count(void)
{
    return handled_faults;
}

#include "tss.h"

#include "lib/string.h"

static tss_t boot_tss;

void tss_init(void)
{
    memset(&boot_tss, 0, sizeof(boot_tss));
    boot_tss.iopb_offset = sizeof(boot_tss);
}

void tss_set_kernel_stack(uint64_t stack)
{
    boot_tss.rsp[0] = stack;
}

tss_t *tss_get(void)
{
    return &boot_tss;
}

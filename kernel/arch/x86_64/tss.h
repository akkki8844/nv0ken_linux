#ifndef NV0KEN_ARCH_X86_64_TSS_H
#define NV0KEN_ARCH_X86_64_TSS_H

#include <stdint.h>

typedef struct {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss_t;

void tss_init(void);
void tss_set_kernel_stack(uint64_t stack);
tss_t *tss_get(void);

#endif

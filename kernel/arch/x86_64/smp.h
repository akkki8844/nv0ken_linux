#ifndef NV0KEN_ARCH_X86_64_SMP_H
#define NV0KEN_ARCH_X86_64_SMP_H

#include <stdint.h>

#define SMP_MAX_CPUS 64

typedef struct {
    uint32_t processor_id;
    uint32_t lapic_id;
    uint8_t bootstrap;
    uint8_t online;
} smp_cpu_t;

void smp_init(void);
int smp_register_cpu(uint32_t processor_id, uint32_t lapic_id, int bootstrap);
unsigned smp_cpu_count(void);
const smp_cpu_t *smp_get_cpu(unsigned index);

#endif

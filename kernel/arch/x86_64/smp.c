#include "smp.h"

#include "lib/string.h"

static smp_cpu_t cpus[SMP_MAX_CPUS];
static unsigned cpu_count;

void smp_init(void)
{
    memset(cpus, 0, sizeof(cpus));
    cpu_count = 0;
    smp_register_cpu(0, 0, 1);
}

int smp_register_cpu(uint32_t processor_id, uint32_t lapic_id, int bootstrap)
{
    if (cpu_count >= SMP_MAX_CPUS) {
        return -1;
    }
    cpus[cpu_count].processor_id = processor_id;
    cpus[cpu_count].lapic_id = lapic_id;
    cpus[cpu_count].bootstrap = bootstrap ? 1 : 0;
    cpus[cpu_count].online = bootstrap ? 1 : 0;
    return (int)cpu_count++;
}

unsigned smp_cpu_count(void)
{
    return cpu_count;
}

const smp_cpu_t *smp_get_cpu(unsigned index)
{
    if (index >= cpu_count) {
        return 0;
    }
    return &cpus[index];
}

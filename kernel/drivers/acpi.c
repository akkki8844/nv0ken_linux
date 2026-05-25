#include "acpi.h"

#include "lib/log.h"

static const acpi_rsdp_t *boot_rsdp;

int acpi_checksum(const void *table, uint32_t length)
{
    const uint8_t *bytes = table;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < length; ++i) {
        sum = (uint8_t)(sum + bytes[i]);
    }
    return sum == 0;
}

void acpi_init(void *rsdp)
{
    boot_rsdp = rsdp;
    if (!boot_rsdp) {
        log_warn("acpi", "RSDP not supplied");
        return;
    }
    if (!acpi_checksum(boot_rsdp, 20)) {
        log_error("acpi", "RSDP checksum failed");
        boot_rsdp = 0;
        return;
    }
    log_info("acpi", "RSDP detected revision %u", boot_rsdp->revision);
}

const acpi_rsdp_t *acpi_rsdp(void)
{
    return boot_rsdp;
}

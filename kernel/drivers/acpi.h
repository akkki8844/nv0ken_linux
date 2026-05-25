#ifndef NV0KEN_DRIVERS_ACPI_H
#define NV0KEN_DRIVERS_ACPI_H

#include <stdint.h>

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) acpi_rsdp_t;

void acpi_init(void *rsdp);
const acpi_rsdp_t *acpi_rsdp(void);
int acpi_checksum(const void *table, uint32_t length);

#endif

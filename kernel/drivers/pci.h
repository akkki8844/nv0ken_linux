#ifndef NV0KEN_DRIVERS_PCI_H
#define NV0KEN_DRIVERS_PCI_H

#include <stdint.h>

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
} pci_device_t;

typedef void (*pci_visit_fn)(const pci_device_t *device, void *context);

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value);
uint16_t pci_vendor_id(uint8_t bus, uint8_t slot, uint8_t function);
void pci_scan(pci_visit_fn visit, void *context);

#endif

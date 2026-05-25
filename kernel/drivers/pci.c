#include "pci.h"

#include "arch/x86_64/io.h"

#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA    0xcfc

static uint32_t pci_address(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    return 0x80000000u |
           ((uint32_t)bus << 16) |
           ((uint32_t)slot << 11) |
           ((uint32_t)function << 8) |
           (offset & 0xfc);
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, slot, function, offset));
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function,
                        uint8_t offset, uint32_t value)
{
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, slot, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

uint16_t pci_vendor_id(uint8_t bus, uint8_t slot, uint8_t function)
{
    return (uint16_t)(pci_config_read32(bus, slot, function, 0) & 0xffff);
}

void pci_scan(pci_visit_fn visit, void *context)
{
    if (!visit) {
        return;
    }
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t function = 0; function < 8; ++function) {
                uint32_t id = pci_config_read32((uint8_t)bus, slot, function, 0);
                if ((id & 0xffff) == 0xffff) {
                    continue;
                }
                uint32_t class_reg = pci_config_read32((uint8_t)bus, slot, function, 8);
                pci_device_t dev = {
                    .bus = (uint8_t)bus,
                    .slot = slot,
                    .function = function,
                    .vendor_id = (uint16_t)(id & 0xffff),
                    .device_id = (uint16_t)(id >> 16),
                    .revision = (uint8_t)class_reg,
                    .prog_if = (uint8_t)(class_reg >> 8),
                    .subclass = (uint8_t)(class_reg >> 16),
                    .class_code = (uint8_t)(class_reg >> 24),
                };
                visit(&dev, context);
            }
        }
    }
}

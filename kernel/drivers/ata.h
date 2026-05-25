#ifndef NV0KEN_DRIVERS_ATA_H
#define NV0KEN_DRIVERS_ATA_H

#include <stddef.h>
#include <stdint.h>

#define ATA_SECTOR_SIZE 512

int ata_read28(uint8_t drive, uint32_t lba, uint8_t sectors, void *buffer);
int ata_write28(uint8_t drive, uint32_t lba, uint8_t sectors, const void *buffer);

#endif

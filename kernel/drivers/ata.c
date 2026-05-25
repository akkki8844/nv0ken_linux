#include "ata.h"

#include "arch/x86_64/io.h"

#define ATA_DATA 0x1f0
#define ATA_SECCOUNT 0x1f2
#define ATA_LBA0 0x1f3
#define ATA_LBA1 0x1f4
#define ATA_LBA2 0x1f5
#define ATA_DRIVE 0x1f6
#define ATA_STATUS 0x1f7
#define ATA_COMMAND 0x1f7

#define ATA_CMD_READ 0x20
#define ATA_CMD_WRITE 0x30

static int ata_wait_ready(void)
{
    for (unsigned i = 0; i < 100000; ++i) {
        uint8_t status = inb(ATA_STATUS);
        if ((status & 0x80) == 0 && (status & 0x08)) {
            return 0;
        }
        if (status & 0x01) {
            return -1;
        }
    }
    return -1;
}

static void ata_select(uint8_t drive, uint32_t lba, uint8_t sectors)
{
    outb(ATA_DRIVE, (uint8_t)(0xe0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0f)));
    outb(ATA_SECCOUNT, sectors);
    outb(ATA_LBA0, (uint8_t)lba);
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
}

int ata_read28(uint8_t drive, uint32_t lba, uint8_t sectors, void *buffer)
{
    uint16_t *out = buffer;
    ata_select(drive, lba, sectors);
    outb(ATA_COMMAND, ATA_CMD_READ);
    for (uint8_t s = 0; s < sectors; ++s) {
        if (ata_wait_ready() < 0) {
            return -1;
        }
        for (unsigned i = 0; i < ATA_SECTOR_SIZE / 2; ++i) {
            *out++ = inw(ATA_DATA);
        }
    }
    return 0;
}

int ata_write28(uint8_t drive, uint32_t lba, uint8_t sectors, const void *buffer)
{
    const uint16_t *in = buffer;
    ata_select(drive, lba, sectors);
    outb(ATA_COMMAND, ATA_CMD_WRITE);
    for (uint8_t s = 0; s < sectors; ++s) {
        if (ata_wait_ready() < 0) {
            return -1;
        }
        for (unsigned i = 0; i < ATA_SECTOR_SIZE / 2; ++i) {
            outw(ATA_DATA, *in++);
        }
    }
    return 0;
}

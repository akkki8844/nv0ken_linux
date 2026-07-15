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

#define ATA_CMD_IDENTIFY 0xec
#define ATA_CMD_READ 0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_CACHE_FLUSH 0xe7

#define ATA_STATUS_ERR 0x01
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_DF 0x20
#define ATA_STATUS_BSY 0x80

static void ata_select(uint8_t drive, uint32_t lba, uint8_t sectors);

static int ata_wait_ready(void)
{
    for (unsigned i = 0; i < 100000; ++i) {
        uint8_t status = inb(ATA_STATUS);
        if ((status & ATA_STATUS_BSY) == 0 && (status & ATA_STATUS_DRQ)) {
            return 0;
        }
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
            return -1;
        }
    }
    return -1;
}

static int ata_validate_request(uint32_t lba, uint8_t sectors, const void *buffer)
{
    if (!buffer || sectors == 0 || lba > 0x0fffffffu) {
        return -1;
    }
    if ((uint32_t)sectors - 1 > 0x0fffffffu - lba) {
        return -1;
    }
    return 0;
}

int ata_device_present(uint8_t drive)
{
    if (drive > 1) {
        return 0;
    }

    ata_select(drive, 0, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    uint8_t status = inb(ATA_STATUS);
    if (status == 0) {
        return 0;
    }
    for (unsigned index = 0; index < 100000; ++index) {
        status = inb(ATA_STATUS);
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
            return 0;
        }
        if ((status & ATA_STATUS_BSY) == 0 && (status & ATA_STATUS_DRQ)) {
            for (unsigned word = 0; word < ATA_SECTOR_SIZE / 2; ++word) {
                (void)inw(ATA_DATA);
            }
            return 1;
        }
    }
    return 0;
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
    if (drive > 1 || ata_validate_request(lba, sectors, buffer) != 0) {
        return -1;
    }
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
    if (drive > 1 || ata_validate_request(lba, sectors, buffer) != 0) {
        return -1;
    }
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
    return ata_flush(drive);
}

int ata_flush(uint8_t drive)
{
    if (drive > 1) {
        return -1;
    }
    outb(ATA_DRIVE, (uint8_t)(0xe0 | (drive << 4)));
    outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
    for (unsigned index = 0; index < 100000; ++index) {
        uint8_t status = inb(ATA_STATUS);
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
            return -1;
        }
        if ((status & ATA_STATUS_BSY) == 0) {
            return 0;
        }
    }
    return -1;
}

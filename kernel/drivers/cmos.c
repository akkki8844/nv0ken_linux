#include "cmos.h"

#include "arch/x86_64/io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static uint8_t bcd_to_bin(uint8_t value)
{
    return (uint8_t)((value & 0x0f) + ((value >> 4) * 10));
}

uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDR, (uint8_t)(reg | 0x80));
    io_wait();
    return inb(CMOS_DATA);
}

void cmos_read_time(cmos_time_t *time)
{
    if (!time) {
        return;
    }

    uint8_t status_b = cmos_read(0x0b);
    int bcd = (status_b & 0x04) == 0;

    time->second = cmos_read(0x00);
    time->minute = cmos_read(0x02);
    time->hour = cmos_read(0x04);
    time->day = cmos_read(0x07);
    time->month = cmos_read(0x08);
    time->year = (uint16_t)cmos_read(0x09);

    if (bcd) {
        time->second = bcd_to_bin(time->second);
        time->minute = bcd_to_bin(time->minute);
        time->hour = bcd_to_bin(time->hour);
        time->day = bcd_to_bin(time->day);
        time->month = bcd_to_bin(time->month);
        time->year = bcd_to_bin((uint8_t)time->year);
    }

    time->year += 2000;
}

#include "rtc.h"

#include "../arch/x86_64/io.h"

#define CMOS_INDEX 0x70
#define CMOS_DATA 0x71

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_INDEX, (uint8_t)(0x80 | reg));
    return inb(CMOS_DATA);
}

static uint8_t bcd_to_binary(uint8_t value)
{
    return (uint8_t)((value & 0x0f) + ((value >> 4) * 10));
}

static int rtc_updating(void)
{
    return (cmos_read(0x0a) & 0x80) != 0;
}

static void read_snapshot(rtc_time_t *time, uint8_t *status_b)
{
    time->second = cmos_read(0x00);
    time->minute = cmos_read(0x02);
    time->hour = cmos_read(0x04);
    time->day = cmos_read(0x07);
    time->month = cmos_read(0x08);
    time->year = cmos_read(0x09);
    *status_b = cmos_read(0x0b);
}

static int same_time(const rtc_time_t *left, const rtc_time_t *right)
{
    return left->second == right->second && left->minute == right->minute &&
           left->hour == right->hour && left->day == right->day &&
           left->month == right->month && left->year == right->year;
}

int rtc_read(rtc_time_t *time)
{
    if (!time) {
        return -1;
    }

    rtc_time_t previous;
    uint8_t status_b = 0;
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        while (rtc_updating()) {
        }
        read_snapshot(&previous, &status_b);
        while (rtc_updating()) {
        }
        read_snapshot(time, &status_b);
        if (same_time(&previous, time)) {
            break;
        }
        if (attempt == 7) {
            return -1;
        }
    }

    if ((status_b & 0x04) == 0) {
        time->second = bcd_to_binary(time->second);
        time->minute = bcd_to_binary(time->minute);
        time->hour = (uint8_t)(bcd_to_binary(time->hour & 0x7f) |
                               (time->hour & 0x80));
        time->day = bcd_to_binary(time->day);
        time->month = bcd_to_binary(time->month);
        time->year = bcd_to_binary((uint8_t)time->year);
    }

    if ((status_b & 0x02) == 0) {
        uint8_t afternoon = time->hour & 0x80;
        time->hour &= 0x7f;
        if (afternoon) {
            time->hour = (uint8_t)((time->hour % 12) + 12);
        } else if (time->hour == 12) {
            time->hour = 0;
        }
    }

    time->year = (uint16_t)(2000 + time->year);
    return 0;
}

static int leap_year(uint64_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

uint64_t rtc_unix_time(void)
{
    rtc_time_t time;
    if (rtc_read(&time) != 0 || time.year < 1970 || time.month < 1 ||
        time.month > 12 || time.day < 1 || time.day > 31) {
        return 0;
    }

    static const uint16_t days_before_month[] = {
        0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334,
    };
    uint64_t days = 0;
    for (uint64_t year = 1970; year < time.year; ++year) {
        days += leap_year(year) ? 366 : 365;
    }
    days += days_before_month[time.month] + time.day - 1;
    if (time.month > 2 && leap_year(time.year)) {
        ++days;
    }
    return days * 86400ull + (uint64_t)time.hour * 3600ull +
           (uint64_t)time.minute * 60ull + time.second;
}

#ifndef NV0KEN_DRIVERS_RTC_H
#define NV0KEN_DRIVERS_RTC_H

#include <stdint.h>

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_time_t;

int rtc_read(rtc_time_t *time);
uint64_t rtc_unix_time(void);

#endif

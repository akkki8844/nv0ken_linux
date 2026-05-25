#ifndef NV0KEN_DRIVERS_CMOS_H
#define NV0KEN_DRIVERS_CMOS_H

#include <stdint.h>

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} cmos_time_t;

uint8_t cmos_read(uint8_t reg);
void cmos_read_time(cmos_time_t *time);

#endif

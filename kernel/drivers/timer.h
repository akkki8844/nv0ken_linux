#ifndef NV0KEN_DRIVERS_TIMER_H
#define NV0KEN_DRIVERS_TIMER_H

#include <stdint.h>

typedef void (*timer_callback_t)(void *context);

void timer_init(uint32_t hz);
uint64_t timer_ticks(void);
uint64_t timer_uptime_ms(void);
void timer_sleep_ms(uint64_t ms);
void timer_set_callback(timer_callback_t cb, void *context);

#endif

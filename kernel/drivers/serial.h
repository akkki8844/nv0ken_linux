#ifndef NV0KEN_DRIVERS_SERIAL_H
#define NV0KEN_DRIVERS_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

#define COM1 0x3f8

bool serial_init(uint16_t port);
bool serial_ready(void);
void serial_write_char(char ch);
void serial_write(const char *text);

#endif

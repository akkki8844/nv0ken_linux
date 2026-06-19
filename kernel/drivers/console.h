#ifndef NV0KEN_DRIVERS_CONSOLE_H
#define NV0KEN_DRIVERS_CONSOLE_H

#include <stddef.h>

void console_init(void);
size_t console_read(void *buffer, size_t count, int blocking);
size_t console_pending(void);

#endif

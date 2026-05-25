#ifndef NV0KEN_DRIVERS_MOUSE_H
#define NV0KEN_DRIVERS_MOUSE_H

#include <stdint.h>

typedef struct {
    int dx;
    int dy;
    uint8_t buttons;
} mouse_packet_t;

typedef void (*mouse_event_fn)(const mouse_packet_t *packet, void *context);

void mouse_init(void);
void mouse_set_handler(mouse_event_fn handler, void *context);

#endif

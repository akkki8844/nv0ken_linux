#ifndef NV0KEN_DRIVERS_KEYBOARD_H
#define NV0KEN_DRIVERS_KEYBOARD_H

#include <stdint.h>

typedef void (*keyboard_event_fn)(uint8_t scancode, char ch, int pressed, void *context);

void keyboard_init(void);
void keyboard_set_handler(keyboard_event_fn handler, void *context);
int keyboard_shift_down(void);

#endif

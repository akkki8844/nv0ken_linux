#include "keyboard.h"

#include "arch/x86_64/io.h"
#include "arch/x86_64/irq.h"
#include "keyboard_map.h"

#define KBD_DATA 0x60
#define KBD_STATUS 0x64

static keyboard_event_fn event_handler;
static void *event_context;
static int shift_down;

static void keyboard_irq(registers_t *regs, void *context)
{
    (void)regs;
    (void)context;
    uint8_t scancode = inb(KBD_DATA);
    int pressed = (scancode & 0x80) == 0;
    uint8_t code = scancode & 0x7f;

    if (code == 0x2a || code == 0x36) {
        shift_down = pressed;
        return;
    }

    char ch = shift_down ? keyboard_us_shift_map[code] : keyboard_us_map[code];
    if (event_handler) {
        event_handler(code, ch, pressed, event_context);
    }
}

void keyboard_init(void)
{
    for (unsigned spin = 0; spin < 256 && (inb(KBD_STATUS) & 1); ++spin) {
        (void)inb(KBD_DATA);
    }
    irq_register(1, keyboard_irq, 0);
}

void keyboard_set_handler(keyboard_event_fn handler, void *context)
{
    event_handler = handler;
    event_context = context;
}

int keyboard_shift_down(void)
{
    return shift_down;
}

int keyboard_poll_char(char *out)
{
    if (!out || (inb(KBD_STATUS) & 1) == 0) {
        return 0;
    }

    uint8_t scancode = inb(KBD_DATA);
    int pressed = (scancode & 0x80) == 0;
    uint8_t code = scancode & 0x7f;
    if (code == 0x2a || code == 0x36) {
        shift_down = pressed;
        return 0;
    }
    if (!pressed) {
        return 0;
    }

    char ch = shift_down ? keyboard_us_shift_map[code] : keyboard_us_map[code];
    if (!ch) {
        return 0;
    }
    *out = ch;
    return 1;
}

#include "console.h"

#include "keyboard.h"
#include "lib/kprintf.h"

#define CONSOLE_INPUT_CAPACITY 512

static volatile unsigned char input_buffer[CONSOLE_INPUT_CAPACITY];
static volatile size_t input_head;
static volatile size_t input_tail;

static size_t next_index(size_t index)
{
    return (index + 1) % CONSOLE_INPUT_CAPACITY;
}

static void console_keyboard_event(uint8_t scancode, char ch, int pressed, void *context)
{
    (void)scancode;
    (void)context;
    if (!pressed || ch == 0) {
        return;
    }

    if (ch == '\b') {
        if (input_head != input_tail) {
            input_head = input_head == 0 ? CONSOLE_INPUT_CAPACITY - 1 : input_head - 1;
            kputs("\b \b");
        }
        return;
    }

    size_t next = next_index(input_head);
    if (next == input_tail) {
        return;
    }
    input_buffer[input_head] = (unsigned char)ch;
    input_head = next;
    kputchar(ch);
}

void console_init(void)
{
    input_head = 0;
    input_tail = 0;
    keyboard_set_handler(console_keyboard_event, 0);
}

size_t console_pending(void)
{
    size_t head = input_head;
    size_t tail = input_tail;
    return head >= tail ? head - tail : CONSOLE_INPUT_CAPACITY - tail + head;
}

size_t console_read(void *buffer, size_t count, int blocking)
{
    unsigned char *out = buffer;
    size_t read = 0;
    if (!out || count == 0) {
        return 0;
    }

    while (read < count) {
        while (input_tail == input_head) {
            if (!blocking || read > 0) {
                return read;
            }
            char polled;
            if (keyboard_poll_char(&polled)) {
                out[read++] = (unsigned char)polled;
                kputchar(polled);
                if (polled == '\n' || read == count) {
                    return read;
                }
            } else {
                __asm__ volatile("pause");
            }
        }

        unsigned char ch = input_buffer[input_tail];
        input_tail = next_index(input_tail);
        out[read++] = ch;
        if (ch == '\n') {
            break;
        }
    }
    return read;
}

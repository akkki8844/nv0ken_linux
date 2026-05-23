#include <limine.h>

#include "drivers/framebuffer.h"
#include "drivers/serial.h"
#include "lib/kprintf.h"

LIMINE_BASE_REVISION(2);

LIMINE_REQUEST static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = { LIMINE_FRAMEBUFFER_REQUEST_ID },
    .revision = 0,
    .response = 0,
};

LIMINE_REQUEST_TERMINATE;

static void halt_forever(void)
{
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void kmain(void)
{
    serial_init(COM1);

    struct limine_framebuffer_response *fb_response = framebuffer_request.response;
    if (fb_response && fb_response->framebuffer_count > 0) {
        framebuffer_init(fb_response->framebuffers[0]);
    }

    framebuffer_clear(0x101820);

    kprintf("nv0ken_linux booted\n");
    kprintf("limine framebuffer: %ux%u\n",
            framebuffer_width(),
            framebuffer_height());
    kprintf("kernel baseline online\n");

    halt_forever();
}

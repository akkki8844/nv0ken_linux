#include "desktop.h"

#include "drivers/framebuffer.h"
#include "lib/kprintf.h"
#include "mm/heap.h"
#include "mm/pmm.h"

#define COLOR_WALLPAPER 0x182335
#define COLOR_PANEL     0x202d42
#define COLOR_CARD      0x263750
#define COLOR_BORDER    0x4a6587
#define COLOR_ACCENT    0x63b3ed
#define COLOR_TEXT      0xe7eef8
#define COLOR_MUTED     0x9fb3c8
#define COLOR_TERMINAL  0x101820

static void label(uint32_t x, uint32_t y, const char *text, uint32_t color,
                  uint32_t background)
{
    framebuffer_set_console_style(color, background, x, y);
    framebuffer_write(text);
}

static void card(uint32_t x, uint32_t y, uint32_t width, const char *title,
                 const char *value)
{
    framebuffer_fill_rect(x, y, width, 50, COLOR_CARD);
    framebuffer_stroke_rect(x, y, width, 50, COLOR_BORDER);
    label(x + 10, y + 9, title, COLOR_MUTED, COLOR_CARD);
    label(x + 10, y + 28, value, COLOR_TEXT, COLOR_CARD);
}

void desktop_render(uint32_t pci_devices)
{
    if (!framebuffer_ready()) {
        return;
    }
    uint32_t width = framebuffer_width();
    uint32_t height = framebuffer_height();
    framebuffer_clear(COLOR_WALLPAPER);

    if (width < 480 || height < 240) {
        framebuffer_fill_rect(0, 0, width, 24, COLOR_PANEL);
        framebuffer_set_console_style(COLOR_TEXT, COLOR_PANEL, 8, 8);
        framebuffer_write("NV0KEN RECOVERY TERMINAL");
        framebuffer_fill_rect(4, 30, width - 8, height - 34, COLOR_TERMINAL);
        framebuffer_stroke_rect(4, 30, width - 8, height - 34, COLOR_BORDER);
        framebuffer_set_console_style(COLOR_TEXT, COLOR_TERMINAL, 12, 40);
        framebuffer_set_console_viewport(12, 40, width - 24, height - 50);
        return;
    }

    framebuffer_fill_rect(0, 0, width, 36, COLOR_PANEL);
    framebuffer_fill_rect(0, 35, width, 1, COLOR_ACCENT);
    framebuffer_set_console_style(COLOR_TEXT, COLOR_PANEL, 16, 13);
    framebuffer_write("NV0KEN  /  RECOVERY DESKTOP");
    framebuffer_set_console_style(COLOR_MUTED, COLOR_PANEL,
                                 width > 180 ? width - 166 : 0, 13);
    framebuffer_write("SYSTEM ONLINE");

    uint32_t margin = 20;
    uint32_t available = width > margin * 2 ? width - margin * 2 : width;
    uint32_t card_width = available / 3;
    card(margin, 56, card_width - 8, "MEMORY", "kernel allocator ready");
    card(margin + card_width, 56, card_width - 8, "STORAGE", "initrd mounted");
    card(margin + card_width * 2, 56, card_width - 8, "HARDWARE", "PCI scan complete");

    uint32_t terminal_y = 124;
    uint32_t terminal_height = height - terminal_y - 18;
    framebuffer_fill_rect(margin, terminal_y, available, terminal_height, COLOR_TERMINAL);
    framebuffer_stroke_rect(margin, terminal_y, available, terminal_height, COLOR_BORDER);
    framebuffer_fill_rect(margin, terminal_y, available, 24, COLOR_PANEL);
    framebuffer_set_console_style(COLOR_TEXT, COLOR_PANEL, margin + 12, terminal_y + 8);
    framebuffer_write("RECOVERY TERMINAL");
    framebuffer_set_console_style(COLOR_MUTED, COLOR_PANEL, margin + 132, terminal_y + 8);
    kprintf("  %u PCI devices  |  %zu free frames", pci_devices, pmm_free_count());

    framebuffer_set_console_style(COLOR_TEXT, COLOR_TERMINAL, margin + 12, terminal_y + 34);
    framebuffer_set_console_viewport(margin + 12, terminal_y + 34,
                                     available - 24, terminal_height - 44);
}

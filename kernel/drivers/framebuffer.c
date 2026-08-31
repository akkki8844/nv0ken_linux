#include "framebuffer.h"

#include <stddef.h>

#include "../lib/string.h"

#define FONT_WIDTH 5u
#define FONT_HEIGHT 7u
#define LINE_HEIGHT 10u

struct framebuffer_state {
    uint32_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t pitch_pixels;
    uint32_t fg;
    uint32_t bg;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t viewport_x;
    uint32_t viewport_y;
    uint32_t viewport_width;
    uint32_t viewport_height;
};

static struct framebuffer_state fb;

static const uint8_t font5x7[][7] = {
    ['0'] = {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e},
    ['1'] = {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e},
    ['2'] = {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f},
    ['3'] = {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e},
    ['4'] = {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02},
    ['5'] = {0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e},
    ['6'] = {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e},
    ['7'] = {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    ['8'] = {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e},
    ['9'] = {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c},
    ['A'] = {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
    ['B'] = {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e},
    ['C'] = {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e},
    ['D'] = {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e},
    ['E'] = {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f},
    ['F'] = {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10},
    ['G'] = {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f},
    ['H'] = {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
    ['I'] = {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e},
    ['J'] = {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c},
    ['K'] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    ['L'] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f},
    ['M'] = {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11},
    ['N'] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    ['O'] = {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
    ['P'] = {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10},
    ['Q'] = {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d},
    ['R'] = {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11},
    ['S'] = {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e},
    ['T'] = {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    ['U'] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
    ['V'] = {0x11, 0x11, 0x11, 0x11, 0x0a, 0x0a, 0x04},
    ['W'] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11},
    ['X'] = {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11},
    ['Y'] = {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04},
    ['Z'] = {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f},
    ['_'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f},
    ['-'] = {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00},
    [':'] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00},
    ['x'] = {0x00, 0x00, 0x11, 0x0a, 0x04, 0x0a, 0x11},
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

static char normalize_char(char ch)
{
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - 'a' + 'A');
    }
    return ch;
}

static void put_pixel(uint32_t x, uint32_t y, uint32_t rgb)
{
    if (!framebuffer_ready() || x >= fb.width || y >= fb.height) {
        return;
    }

    fb.pixels[y * fb.pitch_pixels + x] = rgb;
}

bool framebuffer_init(struct limine_framebuffer *limine_fb)
{
    if (!limine_fb || !limine_fb->address || limine_fb->bpp != 32) {
        return false;
    }

    return framebuffer_init_raw((uint64_t)(uintptr_t)limine_fb->address,
                                (uint32_t)limine_fb->width,
                                (uint32_t)limine_fb->height,
                                (uint32_t)limine_fb->pitch,
                                (uint8_t)limine_fb->bpp);
}

bool framebuffer_init_raw(uint64_t address, uint32_t width, uint32_t height,
                          uint32_t pitch, uint8_t bpp)
{
    if (!address || !width || !height || bpp != 32 || pitch < width * 4) {
        return false;
    }

    fb.pixels = (uint32_t *)(uintptr_t)address;
    fb.width = width;
    fb.height = height;
    fb.pitch_pixels = pitch / 4;
    fb.fg = 0xd8e7ff;
    fb.bg = 0x101820;
    fb.cursor_x = 2;
    fb.cursor_y = 2;
    fb.viewport_x = 0;
    fb.viewport_y = 0;
    fb.viewport_width = width;
    fb.viewport_height = height;
    return true;
}

bool framebuffer_ready(void)
{
    return fb.pixels != NULL;
}

uint32_t framebuffer_width(void)
{
    return fb.width;
}

uint32_t framebuffer_height(void)
{
    return fb.height;
}

void framebuffer_clear(uint32_t rgb)
{
    if (!framebuffer_ready()) {
        return;
    }

    for (uint32_t y = 0; y < fb.height; ++y) {
        for (uint32_t x = 0; x < fb.width; ++x) {
            put_pixel(x, y, rgb);
        }
    }

    fb.bg = rgb;
    fb.cursor_x = 2;
    fb.cursor_y = 2;
    fb.viewport_x = 0;
    fb.viewport_y = 0;
    fb.viewport_width = fb.width;
    fb.viewport_height = fb.height;
}

void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                           uint32_t rgb)
{
    if (!framebuffer_ready() || x >= fb.width || y >= fb.height) {
        return;
    }
    if (width > fb.width - x) {
        width = fb.width - x;
    }
    if (height > fb.height - y) {
        height = fb.height - y;
    }
    for (uint32_t row = y; row < y + height; ++row) {
        for (uint32_t column = x; column < x + width; ++column) {
            fb.pixels[row * fb.pitch_pixels + column] = rgb;
        }
    }
}

void framebuffer_stroke_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                             uint32_t rgb)
{
    if (width == 0 || height == 0) {
        return;
    }
    framebuffer_fill_rect(x, y, width, 1, rgb);
    framebuffer_fill_rect(x, y + height - 1, width, 1, rgb);
    framebuffer_fill_rect(x, y, 1, height, rgb);
    framebuffer_fill_rect(x + width - 1, y, 1, height, rgb);
}

void framebuffer_set_console_style(uint32_t foreground, uint32_t background,
                                   uint32_t cursor_x, uint32_t cursor_y)
{
    fb.fg = foreground;
    fb.bg = background;
    fb.cursor_x = cursor_x < fb.width ? cursor_x : 0;
    fb.cursor_y = cursor_y < fb.height ? cursor_y : 0;
}

void framebuffer_set_console_viewport(uint32_t x, uint32_t y,
                                      uint32_t width, uint32_t height)
{
    if (!framebuffer_ready() || x >= fb.width || y >= fb.height) {
        return;
    }
    if (width > fb.width - x) {
        width = fb.width - x;
    }
    if (height > fb.height - y) {
        height = fb.height - y;
    }
    if (width < FONT_WIDTH + 2 || height < LINE_HEIGHT) {
        return;
    }
    fb.viewport_x = x;
    fb.viewport_y = y;
    fb.viewport_width = width;
    fb.viewport_height = height;
    fb.cursor_x = x;
    fb.cursor_y = y;
}

static void newline(void)
{
    fb.cursor_x = fb.viewport_x;
    fb.cursor_y += LINE_HEIGHT;
    uint32_t viewport_bottom = fb.viewport_y + fb.viewport_height;
    if (fb.cursor_y + FONT_HEIGHT < viewport_bottom) {
        return;
    }

    /* Keep the recovery console usable after a long boot log instead of
     * silently drawing beyond the framebuffer. */
    if (fb.viewport_height <= LINE_HEIGHT) {
        framebuffer_fill_rect(fb.viewport_x, fb.viewport_y,
                              fb.viewport_width, fb.viewport_height, fb.bg);
        fb.cursor_y = fb.viewport_y;
        return;
    }
    uint32_t retained_rows = fb.viewport_height - LINE_HEIGHT;
    for (uint32_t row = 0; row < retained_rows; ++row) {
        uint32_t *destination = fb.pixels + (fb.viewport_y + row) * fb.pitch_pixels + fb.viewport_x;
        uint32_t *source = fb.pixels + (fb.viewport_y + row + LINE_HEIGHT) * fb.pitch_pixels + fb.viewport_x;
        memmove(destination, source, fb.viewport_width * sizeof(*fb.pixels));
    }
    for (uint32_t row = fb.viewport_y + retained_rows; row < viewport_bottom; ++row) {
        for (uint32_t column = fb.viewport_x; column < fb.viewport_x + fb.viewport_width; ++column) {
            fb.pixels[row * fb.pitch_pixels + column] = fb.bg;
        }
    }
    fb.cursor_y = viewport_bottom - LINE_HEIGHT;
}

void framebuffer_putc(char ch)
{
    if (!framebuffer_ready()) {
        return;
    }

    if (ch == '\n') {
        newline();
        return;
    }

    if (fb.cursor_x + FONT_WIDTH + 2 >= fb.viewport_x + fb.viewport_width) {
        newline();
    }

    unsigned glyph_index = (unsigned char)normalize_char(ch);
    if (glyph_index >= sizeof(font5x7) / sizeof(font5x7[0])) {
        glyph_index = ' ';
    }
    const uint8_t *glyph = font5x7[glyph_index];
    for (uint32_t row = 0; row < FONT_HEIGHT; ++row) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < FONT_WIDTH; ++col) {
            uint32_t color = (bits & (1u << (FONT_WIDTH - 1 - col))) ? fb.fg : fb.bg;
            put_pixel(fb.cursor_x + col, fb.cursor_y + row, color);
        }
    }

    fb.cursor_x += 6;
}

void framebuffer_write(const char *text)
{
    if (!text) {
        return;
    }

    while (*text) {
        framebuffer_putc(*text++);
    }
}

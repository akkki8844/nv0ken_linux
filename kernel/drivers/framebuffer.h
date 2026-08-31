#ifndef NV0KEN_DRIVERS_FRAMEBUFFER_H
#define NV0KEN_DRIVERS_FRAMEBUFFER_H

#include <limine.h>
#include <stdbool.h>
#include <stdint.h>

bool framebuffer_init(struct limine_framebuffer *limine_fb);
bool framebuffer_init_raw(uint64_t address, uint32_t width, uint32_t height,
                          uint32_t pitch, uint8_t bpp);
bool framebuffer_ready(void);
uint32_t framebuffer_width(void);
uint32_t framebuffer_height(void);
void framebuffer_clear(uint32_t rgb);
void framebuffer_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                           uint32_t rgb);
void framebuffer_stroke_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                             uint32_t rgb);
void framebuffer_set_console_style(uint32_t foreground, uint32_t background,
                                   uint32_t cursor_x, uint32_t cursor_y);
void framebuffer_set_console_viewport(uint32_t x, uint32_t y,
                                      uint32_t width, uint32_t height);
void framebuffer_putc(char ch);
void framebuffer_write(const char *text);

#endif

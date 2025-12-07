/*
 * VibeOS Framebuffer Driver
 */

#ifndef FB_H
#define FB_H

#include <stdint.h>

// Framebuffer info
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;  // Bytes per row
extern uint32_t *fb_base;  // Pointer to pixel memory

// Initialize framebuffer
int fb_init(void);

// Basic drawing
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_clear(uint32_t color);

// Colors (32-bit ARGB)
#define COLOR_BLACK   0x00000000
#define COLOR_WHITE   0x00FFFFFF
#define COLOR_GREEN   0x0000FF00
#define COLOR_AMBER   0x00FFBF00

// Text drawing
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(uint32_t x, uint32_t y, const char *s, uint32_t fg, uint32_t bg);

#endif

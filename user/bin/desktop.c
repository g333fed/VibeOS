/*
 * VibeOS Desktop
 *
 * Window manager and desktop environment.
 * Classic Mac System 7 aesthetic.
 */

#include "vibe.h"

// ============ Window Manager ============

#define MAX_WINDOWS 16
#define TITLE_HEIGHT 20
#define CLOSE_BOX_SIZE 12
#define CLOSE_BOX_MARGIN 4
#define MENU_HEIGHT 20

// Forward declaration
struct window;
typedef struct window window_t;

struct window {
    int x, y, w, h;
    char title[32];
    int visible;
    void (*draw_content)(window_t *win);
};

static kapi_t *api;
static window_t windows[MAX_WINDOWS];
static int window_count = 0;
static int focused_window = -1;

// Dragging state
static int dragging = 0;
static int drag_window = -1;
static int drag_offset_x = 0;
static int drag_offset_y = 0;

// Previous mouse state
static uint8_t prev_buttons = 0;

// Menu state
static int apple_menu_open = 0;
static int should_quit = 0;

// Apple menu dimensions
#define APPLE_MENU_X 4
#define APPLE_MENU_W 20
#define DROPDOWN_W 160
#define DROPDOWN_ITEM_H 18

// Double buffering
static uint32_t *backbuffer = 0;
static uint32_t screen_width, screen_height;

// ============ Backbuffer Drawing ============

static void bb_put_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < (int)screen_width && y >= 0 && y < (int)screen_height) {
        backbuffer[y * screen_width + x] = color;
    }
}

static void bb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int row = y; row < y + h; row++) {
        for (int col = x; col < x + w; col++) {
            bb_put_pixel(col, row, color);
        }
    }
}

static void bb_hline(int x, int y, int w, uint32_t color) {
    for (int i = 0; i < w; i++) {
        bb_put_pixel(x + i, y, color);
    }
}

static void bb_vline(int x, int y, int h, uint32_t color) {
    for (int i = 0; i < h; i++) {
        bb_put_pixel(x, y + i, color);
    }
}

static void bb_rect_outline(int x, int y, int w, int h, uint32_t color) {
    bb_hline(x, y, w, color);
    bb_hline(x, y + h - 1, w, color);
    bb_vline(x, y, h, color);
    bb_vline(x + w - 1, y, h, color);
}

static void flip_buffer(void) {
    for (uint32_t i = 0; i < screen_width * screen_height; i++) {
        api->fb_base[i] = backbuffer[i];
    }
}

// ============ Backbuffer Text Drawing ============

#define FONT_WIDTH 8
#define FONT_HEIGHT 16

static void bb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    const uint8_t *glyph = api->font_data + (unsigned char)c * 16;
    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            bb_put_pixel(x + col, y + row, color);
        }
    }
}

static void bb_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg) {
    while (*s) {
        bb_draw_char(x, y, *s, fg, bg);
        x += FONT_WIDTH;
        s++;
    }
}

// ============ Desktop Pattern ============

static void draw_desktop_pattern(void) {
    uint32_t total = screen_width * screen_height;
    for (uint32_t i = 0; i < total; i++) {
        backbuffer[i] = 0x00808080;
    }
}

// ============ Apple Icon (12x14 bitmap) ============

#define APPLE_ICON_W 12
#define APPLE_ICON_H 14

static const uint8_t apple_icon[APPLE_ICON_H][APPLE_ICON_W] = {
    {0,0,0,0,0,0,1,1,0,0,0,0},
    {0,0,0,0,0,1,1,0,0,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {0,1,1,1,1,1,1,1,1,1,1,0},
    {0,1,1,1,1,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,1,1,1,1,1,1,0,0,0},
    {0,0,0,0,1,1,1,1,0,0,0,0},
};

static void draw_apple_icon(int x, int y) {
    for (int row = 0; row < APPLE_ICON_H; row++) {
        for (int col = 0; col < APPLE_ICON_W; col++) {
            if (apple_icon[row][col]) {
                bb_put_pixel(x + col, y + row, COLOR_BLACK);
            }
        }
    }
}

// ============ Menu Bar ============

static void draw_menu_bar(void) {
    bb_fill_rect(0, 0, screen_width, MENU_HEIGHT, COLOR_WHITE);
    bb_hline(0, MENU_HEIGHT - 1, screen_width, COLOR_BLACK);

    // Draw apple icon
    draw_apple_icon(APPLE_MENU_X, 3);
}

static void draw_apple_dropdown(void) {
    if (!apple_menu_open) return;

    int x = APPLE_MENU_X;
    int y = MENU_HEIGHT;
    int h = DROPDOWN_ITEM_H * 3 + 4;  // 2 items + separator + padding

    // Dropdown background with shadow
    bb_fill_rect(x + 2, y + 2, DROPDOWN_W, h, 0x00000000);
    bb_fill_rect(x, y, DROPDOWN_W, h, COLOR_WHITE);
    bb_rect_outline(x, y, DROPDOWN_W, h, COLOR_BLACK);

    // "About VibeOS..." item
    bb_fill_rect(x + 1, y + 2, DROPDOWN_W - 2, DROPDOWN_ITEM_H, COLOR_WHITE);
    bb_draw_string(x + 8, y + 4, "About VibeOS...", COLOR_BLACK, COLOR_WHITE);

    // Separator line
    bb_hline(x + 1, y + 2 + DROPDOWN_ITEM_H + 2, DROPDOWN_W - 2, COLOR_BLACK);

    // "Quit Desktop" item
    bb_fill_rect(x + 1, y + 4 + DROPDOWN_ITEM_H + 4, DROPDOWN_W - 2, DROPDOWN_ITEM_H, COLOR_WHITE);
    bb_draw_string(x + 8, y + 4 + DROPDOWN_ITEM_H + 8, "Quit Desktop", COLOR_BLACK, COLOR_WHITE);
}

static void draw_menu_text(void) {
    bb_draw_string(30, 3, "File", COLOR_BLACK, COLOR_WHITE);
    bb_draw_string(70, 3, "Edit", COLOR_BLACK, COLOR_WHITE);
    bb_draw_string(110, 3, "View", COLOR_BLACK, COLOR_WHITE);
    bb_draw_string(160, 3, "Special", COLOR_BLACK, COLOR_WHITE);
}

// ============ Window Drawing ============

static void draw_window_frame(window_t *win, int is_focused) {
    int x = win->x, y = win->y, w = win->w, h = win->h;

    // Shadow
    bb_fill_rect(x + 2, y + 2, w, h, 0x00000000);

    // Window background
    bb_fill_rect(x, y, w, h, COLOR_WHITE);

    // Title bar
    if (is_focused) {
        for (int ty = 0; ty < TITLE_HEIGHT; ty++) {
            for (int tx = 0; tx < w; tx++) {
                uint32_t color = (ty % 2 == 0) ? COLOR_WHITE : COLOR_BLACK;
                bb_put_pixel(x + tx, y + ty, color);
            }
        }
    } else {
        bb_fill_rect(x, y, w, TITLE_HEIGHT, COLOR_WHITE);
    }

    // Close box
    int cb_x = x + CLOSE_BOX_MARGIN;
    int cb_y = y + (TITLE_HEIGHT - CLOSE_BOX_SIZE) / 2;
    bb_fill_rect(cb_x, cb_y, CLOSE_BOX_SIZE, CLOSE_BOX_SIZE, COLOR_WHITE);
    bb_rect_outline(cb_x, cb_y, CLOSE_BOX_SIZE, CLOSE_BOX_SIZE, COLOR_BLACK);

    // Title background
    int title_len = 0;
    for (int i = 0; win->title[i]; i++) title_len++;
    bb_fill_rect(x + 28, y + 2, title_len * 8 + 4, 16, COLOR_WHITE);

    // Window border
    bb_rect_outline(x, y, w, h, COLOR_BLACK);

    // Title bar separator
    bb_hline(x, y + TITLE_HEIGHT, w, COLOR_BLACK);
}

static void draw_window_content(window_t *win, int is_focused) {
    int x = win->x, y = win->y, w = win->w, h = win->h;

    // Fill title bar area solid white (prevents bleed-through)
    bb_fill_rect(x + 1, y + 1, w - 2, TITLE_HEIGHT - 1, COLOR_WHITE);

    // Redraw stripes for focused window
    if (is_focused) {
        for (int ty = 1; ty < TITLE_HEIGHT; ty++) {
            if (ty % 2 == 1) {  // Black lines on odd rows
                bb_hline(x + 1, y + ty, w - 2, COLOR_BLACK);
            }
        }
    }

    // Redraw close box
    int cb_x = x + CLOSE_BOX_MARGIN;
    int cb_y = y + (TITLE_HEIGHT - CLOSE_BOX_SIZE) / 2;
    bb_fill_rect(cb_x, cb_y, CLOSE_BOX_SIZE, CLOSE_BOX_SIZE, COLOR_WHITE);
    bb_rect_outline(cb_x, cb_y, CLOSE_BOX_SIZE, CLOSE_BOX_SIZE, COLOR_BLACK);

    // Title text background and text
    int title_len = 0;
    for (int i = 0; win->title[i]; i++) title_len++;
    bb_fill_rect(x + 28, y + 2, title_len * 8 + 4, 16, COLOR_WHITE);
    bb_draw_string(x + 30, y + 4, win->title, COLOR_BLACK, COLOR_WHITE);

    // Fill content area with white
    int content_y = y + TITLE_HEIGHT + 1;
    int content_h = h - TITLE_HEIGHT - 2;
    bb_fill_rect(x + 1, content_y, w - 2, content_h, COLOR_WHITE);

    // Redraw border and title bar separator
    bb_rect_outline(x, y, w, h, COLOR_BLACK);
    bb_hline(x, y + TITLE_HEIGHT, w, COLOR_BLACK);

    // Draw app content
    if (win->draw_content) {
        win->draw_content(win);
    }
}

static void draw_all_windows_frames(void) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i].visible && i != focused_window) {
            draw_window_frame(&windows[i], 0);
        }
    }
    if (focused_window >= 0 && windows[focused_window].visible) {
        draw_window_frame(&windows[focused_window], 1);
    }
}

static void draw_all_windows_text(void) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i].visible && i != focused_window) {
            draw_window_content(&windows[i], 0);
        }
    }
    if (focused_window >= 0 && windows[focused_window].visible) {
        draw_window_content(&windows[focused_window], 1);
    }
}

// ============ Cursor ============

#define CURSOR_W 12
#define CURSOR_H 19

static const uint8_t cursor_data[CURSOR_H][CURSOR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,1,1,1,1,1},
    {1,2,2,2,1,2,2,1,0,0,0,0},
    {1,2,2,1,0,1,2,2,1,0,0,0},
    {1,2,1,0,0,1,2,2,1,0,0,0},
    {1,1,0,0,0,0,1,2,2,1,0,0},
    {1,0,0,0,0,0,1,2,2,1,0,0},
    {0,0,0,0,0,0,0,1,2,1,0,0},
    {0,0,0,0,0,0,0,1,1,0,0,0},
};

static void draw_cursor(int x, int y) {
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            uint8_t p = cursor_data[row][col];
            if (p == 0) continue;
            int px = x + col, py = y + row;
            if (px >= 0 && px < (int)screen_width && py >= 0 && py < (int)screen_height) {
                api->fb_base[py * screen_width + px] = (p == 1) ? COLOR_BLACK : COLOR_WHITE;
            }
        }
    }
}

// ============ Window Management ============

static int create_window(int x, int y, int w, int h, const char *title,
                         void (*draw_content)(window_t *win)) {
    if (window_count >= MAX_WINDOWS) return -1;

    window_t *win = &windows[window_count];
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->visible = 1;
    win->draw_content = draw_content;

    int i;
    for (i = 0; i < 31 && title[i]; i++) {
        win->title[i] = title[i];
    }
    win->title[i] = '\0';

    focused_window = window_count;
    return window_count++;
}

static void close_window(int idx) {
    if (idx < 0 || idx >= window_count) return;
    windows[idx].visible = 0;
    focused_window = -1;
    for (int i = window_count - 1; i >= 0; i--) {
        if (windows[i].visible) {
            focused_window = i;
            break;
        }
    }
}

static int point_in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static int window_at_point(int px, int py) {
    if (focused_window >= 0 && windows[focused_window].visible) {
        window_t *win = &windows[focused_window];
        if (point_in_rect(px, py, win->x, win->y, win->w, win->h)) {
            return focused_window;
        }
    }
    for (int i = window_count - 1; i >= 0; i--) {
        if (i == focused_window) continue;
        if (!windows[i].visible) continue;
        window_t *win = &windows[i];
        if (point_in_rect(px, py, win->x, win->y, win->w, win->h)) {
            return i;
        }
    }
    return -1;
}

// ============ Full Redraw ============

static void redraw_all(int mouse_x, int mouse_y) {
    // Draw everything to backbuffer
    draw_desktop_pattern();
    draw_menu_bar();
    draw_menu_text();
    draw_all_windows_frames();
    draw_all_windows_text();
    draw_apple_dropdown();

    // Flip to screen
    flip_buffer();

    // Draw cursor on top (directly to framebuffer so it's always visible)
    draw_cursor(mouse_x, mouse_y);
}

// ============ Window Content Callbacks ============

static void draw_welcome_content(window_t *win) {
    bb_draw_string(win->x + 20, win->y + TITLE_HEIGHT + 20,
        "Welcome to VibeOS!", COLOR_BLACK, COLOR_WHITE);
    bb_draw_string(win->x + 20, win->y + TITLE_HEIGHT + 40,
        "Drag windows by title bar", COLOR_BLACK, COLOR_WHITE);
    bb_draw_string(win->x + 20, win->y + TITLE_HEIGHT + 60,
        "Click close box to close", COLOR_BLACK, COLOR_WHITE);
    bb_draw_string(win->x + 20, win->y + TITLE_HEIGHT + 80,
        "Use Apple menu to quit", COLOR_BLACK, COLOR_WHITE);
}

static void draw_about_content(window_t *win) {
    bb_draw_string(win->x + 20, win->y + TITLE_HEIGHT + 20,
        "VibeOS v0.1", COLOR_BLACK, COLOR_WHITE);
    bb_draw_string(win->x + 20, win->y + TITLE_HEIGHT + 40,
        "A hobby OS by Claude", COLOR_BLACK, COLOR_WHITE);
    bb_draw_string(win->x + 20, win->y + TITLE_HEIGHT + 60,
        "System 7 vibes", COLOR_BLACK, COLOR_WHITE);
}

// ============ Main Loop ============

static void delay(int cycles) {
    for (volatile int i = 0; i < cycles; i++) {
        __asm__ volatile("nop");
    }
}

int main(kapi_t *kapi, int argc, char **argv) {
    (void)argc;
    (void)argv;

    api = kapi;
    screen_width = api->fb_width;
    screen_height = api->fb_height;

    // Allocate backbuffer
    uint32_t buf_size = screen_width * screen_height * sizeof(uint32_t);
    backbuffer = api->malloc(buf_size);
    if (!backbuffer) {
        api->puts("Failed to allocate backbuffer!\n");
        return 1;
    }

    // Create demo windows
    create_window(50, 80, 300, 200, "Welcome", draw_welcome_content);
    create_window(200, 150, 250, 180, "About VibeOS", draw_about_content);

    // Initial draw
    int mouse_x, mouse_y;
    api->mouse_get_pos(&mouse_x, &mouse_y);
    redraw_all(mouse_x, mouse_y);

    // Main event loop
    while (!should_quit) {
        api->mouse_poll();

        int new_mx, new_my;
        api->mouse_get_pos(&new_mx, &new_my);
        uint8_t buttons = api->mouse_get_buttons();

        int clicked = (buttons & MOUSE_BTN_LEFT) && !(prev_buttons & MOUSE_BTN_LEFT);
        int released = !(buttons & MOUSE_BTN_LEFT) && (prev_buttons & MOUSE_BTN_LEFT);

        int needs_redraw = 0;

        if (dragging) {
            if (released) {
                dragging = 0;
                drag_window = -1;
            } else {
                window_t *win = &windows[drag_window];
                win->x = new_mx - drag_offset_x;
                win->y = new_my - drag_offset_y;
                if (win->y < MENU_HEIGHT) win->y = MENU_HEIGHT;
                needs_redraw = 1;
            }
        } else if (clicked) {
            // Check if clicking on apple icon in menu bar
            if (point_in_rect(new_mx, new_my, APPLE_MENU_X, 0, APPLE_MENU_W, MENU_HEIGHT)) {
                apple_menu_open = !apple_menu_open;
                needs_redraw = 1;
            }
            // Check if clicking in dropdown
            else if (apple_menu_open) {
                int dropdown_x = APPLE_MENU_X;
                int dropdown_y = MENU_HEIGHT;
                int dropdown_h = DROPDOWN_ITEM_H * 3 + 4;

                if (point_in_rect(new_mx, new_my, dropdown_x, dropdown_y, DROPDOWN_W, dropdown_h)) {
                    // Which item?
                    int item_y = new_my - dropdown_y;
                    if (item_y < DROPDOWN_ITEM_H + 4) {
                        // "About VibeOS..." - open About window
                        int has_about = 0;
                        for (int i = 0; i < window_count; i++) {
                            if (windows[i].visible && windows[i].title[0] == 'A') {
                                has_about = 1;
                                focused_window = i;
                                break;
                            }
                        }
                        if (!has_about) {
                            create_window(150, 100, 250, 180, "About VibeOS", draw_about_content);
                        }
                    } else {
                        // "Quit Desktop"
                        should_quit = 1;
                    }
                }
                apple_menu_open = 0;
                needs_redraw = 1;
            }
            // Check windows
            else {
                int win_idx = window_at_point(new_mx, new_my);

                if (win_idx >= 0) {
                    window_t *win = &windows[win_idx];

                    int cb_x = win->x + CLOSE_BOX_MARGIN;
                    int cb_y = win->y + (TITLE_HEIGHT - CLOSE_BOX_SIZE) / 2;

                    if (point_in_rect(new_mx, new_my, cb_x, cb_y, CLOSE_BOX_SIZE, CLOSE_BOX_SIZE)) {
                        close_window(win_idx);
                        needs_redraw = 1;
                    } else if (point_in_rect(new_mx, new_my, win->x, win->y, win->w, TITLE_HEIGHT)) {
                        dragging = 1;
                        drag_window = win_idx;
                        drag_offset_x = new_mx - win->x;
                        drag_offset_y = new_my - win->y;
                        focused_window = win_idx;
                        needs_redraw = 1;
                    } else if (win_idx != focused_window) {
                        focused_window = win_idx;
                        needs_redraw = 1;
                    }
                } else {
                    // Clicked on desktop background - close menu if open
                    if (apple_menu_open) {
                        apple_menu_open = 0;
                        needs_redraw = 1;
                    }
                }
            }
        }

        // Redraw if something changed or mouse moved
        if (needs_redraw || new_mx != mouse_x || new_my != mouse_y) {
            redraw_all(new_mx, new_my);
            mouse_x = new_mx;
            mouse_y = new_my;
        }

        prev_buttons = buttons;

        // Consume any keypresses (for future use)
        while (api->has_key()) {
            api->getc();
        }

        delay(5000);
    }

    // Cleanup
    api->free(backbuffer);
    api->clear();
    api->puts("Desktop exited.\n");

    return 0;
}

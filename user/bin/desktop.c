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
#define DOCK_HEIGHT 50
#define DOCK_ICON_SIZE 32
#define DOCK_PADDING 8

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
#define APPLE_MENU_X 2
#define APPLE_MENU_W 22
#define DROPDOWN_W 160
#define DROPDOWN_ITEM_H 18

// Double buffering
static uint32_t *backbuffer = 0;
static uint32_t screen_width, screen_height;

// Forward declarations
static int point_in_rect(int px, int py, int x, int y, int w, int h);
static int create_window(int x, int y, int w, int h, const char *title,
                         void (*draw_content)(window_t *win));

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

// ============ Calculator Icon (32x32 bitmap) ============

static const uint8_t calc_icon[32][32] = {
    {0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
    {1,1,1,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,1,1,1},
    {1,1,1,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,1,1,1},
    {1,1,1,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,1,1,1},
    {1,1,1,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,1,1,1},
    {1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
    {1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,3,3,3,3,0,0,1,1,1},
    {1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
};

// Colors: 0=transparent, 1=black (outline), 2=green (display), 3=gray (buttons)
static void draw_calc_icon(int x, int y, int highlighted) {
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 32; col++) {
            uint8_t p = calc_icon[row][col];
            uint32_t color;
            switch (p) {
                case 0: continue; // transparent
                case 1: color = COLOR_BLACK; break;
                case 2: color = 0x0040FF40; break; // green display
                case 3: color = highlighted ? 0x00C0C0C0 : 0x00A0A0A0; break; // buttons
                default: continue;
            }
            bb_put_pixel(x + col, y + row, color);
        }
    }
}

// ============ Folder Icon (32x32 bitmap) ============

static const uint8_t folder_icon[32][32] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0},
    {0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

// Colors: 0=transparent, 1=black (outline), 2=yellow (folder body)
static void draw_folder_icon(int x, int y, int highlighted) {
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 32; col++) {
            uint8_t p = folder_icon[row][col];
            uint32_t color;
            switch (p) {
                case 0: continue; // transparent
                case 1: color = COLOR_BLACK; break;
                case 2: color = highlighted ? 0x00FFE060 : 0x00FFCC00; break; // yellow
                default: continue;
            }
            bb_put_pixel(x + col, y + row, color);
        }
    }
}

// ============ Dock ============

#define DOCK_APP_CALC 0
#define DOCK_APP_FILES 1
#define DOCK_APP_COUNT 2

static int dock_hover = -1;

static void draw_dock(void) {
    int dock_y = screen_height - DOCK_HEIGHT;

    // Dock background (slightly raised look)
    bb_fill_rect(0, dock_y, screen_width, DOCK_HEIGHT, 0x00C0C0C0);
    bb_hline(0, dock_y, screen_width, COLOR_WHITE);  // top highlight
    bb_hline(0, dock_y + 1, screen_width, 0x00E0E0E0);

    // Center the icons
    int total_width = DOCK_APP_COUNT * (DOCK_ICON_SIZE + DOCK_PADDING) - DOCK_PADDING;
    int start_x = (screen_width - total_width) / 2;
    // Position icon higher to leave room for label
    int icon_y = dock_y + 4;

    // Draw calculator icon
    draw_calc_icon(start_x, icon_y, dock_hover == DOCK_APP_CALC);
    bb_draw_string(start_x, icon_y + DOCK_ICON_SIZE + 2, "Calc", COLOR_BLACK, 0x00C0C0C0);

    // Draw files icon
    int files_x = start_x + DOCK_ICON_SIZE + DOCK_PADDING;
    draw_folder_icon(files_x, icon_y, dock_hover == DOCK_APP_FILES);
    bb_draw_string(files_x - 4, icon_y + DOCK_ICON_SIZE + 2, "Files", COLOR_BLACK, 0x00C0C0C0);
}

static int dock_hit_test(int mx, int my) {
    int dock_y = screen_height - DOCK_HEIGHT;
    if (my < dock_y) return -1;

    int total_width = DOCK_APP_COUNT * (DOCK_ICON_SIZE + DOCK_PADDING) - DOCK_PADDING;
    int start_x = (screen_width - total_width) / 2;
    int icon_y = dock_y + 4;  // Match draw_dock positioning

    // Check calculator icon
    if (point_in_rect(mx, my, start_x, icon_y, DOCK_ICON_SIZE, DOCK_ICON_SIZE)) {
        return DOCK_APP_CALC;
    }

    // Check files icon
    int files_x = start_x + DOCK_ICON_SIZE + DOCK_PADDING;
    if (point_in_rect(mx, my, files_x, icon_y, DOCK_ICON_SIZE, DOCK_ICON_SIZE)) {
        return DOCK_APP_FILES;
    }

    return -1;
}

// ============ Calculator State ============

static long calc_value = 0;       // Current displayed value
static long calc_operand = 0;     // Stored operand
static char calc_op = 0;          // Pending operation (+, -, *, /)
static int calc_new_input = 1;    // Next digit starts fresh
static int calc_window = -1;      // Window index for calculator

#define CALC_BTN_W 40
#define CALC_BTN_H 30
#define CALC_BTN_GAP 4
#define CALC_DISPLAY_H 30

static void open_calculator(void);

static void calc_clear(void) {
    calc_value = 0;
    calc_operand = 0;
    calc_op = 0;
    calc_new_input = 1;
}

static void calc_digit(int d) {
    if (calc_new_input) {
        calc_value = d;
        calc_new_input = 0;
    } else {
        if (calc_value < 100000000) {  // Prevent overflow
            calc_value = calc_value * 10 + d;
        }
    }
}

static void calc_set_op(char op) {
    if (calc_op && !calc_new_input) {
        // Perform pending operation first
        switch (calc_op) {
            case '+': calc_operand += calc_value; break;
            case '-': calc_operand -= calc_value; break;
            case '*': calc_operand *= calc_value; break;
            case '/': if (calc_value != 0) calc_operand /= calc_value; break;
        }
        calc_value = calc_operand;
    } else {
        calc_operand = calc_value;
    }
    calc_op = op;
    calc_new_input = 1;
}

static void calc_equals(void) {
    if (calc_op) {
        switch (calc_op) {
            case '+': calc_value = calc_operand + calc_value; break;
            case '-': calc_value = calc_operand - calc_value; break;
            case '*': calc_value = calc_operand * calc_value; break;
            case '/': if (calc_value != 0) calc_value = calc_operand / calc_value; break;
        }
        calc_op = 0;
        calc_operand = 0;
    }
    calc_new_input = 1;
}

// Simple itoa for calculator display
static void calc_itoa(long n, char *buf) {
    int neg = 0;
    if (n < 0) {
        neg = 1;
        n = -n;
    }

    char tmp[20];
    int i = 0;
    if (n == 0) {
        tmp[i++] = '0';
    } else {
        while (n > 0) {
            tmp[i++] = '0' + (n % 10);
            n /= 10;
        }
    }

    int j = 0;
    if (neg) buf[j++] = '-';
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

// Calculator button layout - flat strings to avoid PIE relocation issues
// Last row: wide 0 button takes cols 0-1, then = takes cols 2-3 (also wide)
static const char calc_btn_labels[20][4] = {
    "C", "+/-", "%", "/",
    "7", "8", "9", "*",
    "4", "5", "6", "-",
    "1", "2", "3", "+",
    "0", "0", "=", "=",
};

static const char *get_calc_btn(int row, int col) {
    return calc_btn_labels[row * 4 + col];
}

static void draw_calc_button(int x, int y, int w, int h, const char *label, int pressed) {
    // Use white background so black text is clearly visible
    uint32_t bg = pressed ? 0x00C0C0C0 : COLOR_WHITE;
    uint32_t shadow = 0x00404040;
    uint32_t highlight = 0x00F0F0F0;

    bb_fill_rect(x, y, w, h, bg);

    // 3D border effect
    if (!pressed) {
        bb_hline(x, y, w, highlight);
        bb_vline(x, y, h, highlight);
        bb_hline(x, y + h - 1, w, shadow);
        bb_vline(x + w - 1, y, h, shadow);
    } else {
        bb_hline(x, y, w, shadow);
        bb_vline(x, y, h, shadow);
    }

    // Black outline
    bb_rect_outline(x, y, w, h, COLOR_BLACK);

    // Center the label
    int label_len = 0;
    while (label[label_len]) label_len++;
    int tx = x + (w - label_len * FONT_WIDTH) / 2;
    int ty = y + (h - FONT_HEIGHT) / 2;

    // Draw text - black on white should definitely be visible
    for (int i = 0; label[i]; i++) {
        bb_draw_char(tx + i * FONT_WIDTH, ty, label[i], COLOR_BLACK, bg);
    }
}

static void draw_calc_content(window_t *win) {
    int x = win->x + 10;
    int y = win->y + TITLE_HEIGHT + 10;
    int w = win->w - 20;

    // Display area
    bb_fill_rect(x, y, w, CALC_DISPLAY_H, 0x00E0FFE0);  // Light green
    bb_rect_outline(x, y, w, CALC_DISPLAY_H, COLOR_BLACK);

    // Show pending operation on the left side of display
    if (calc_op) {
        char op_str[2] = {calc_op, '\0'};
        bb_draw_string(x + 4, y + (CALC_DISPLAY_H - FONT_HEIGHT) / 2, op_str, 0x00006600, 0x00E0FFE0);
    }

    // Display value (right-aligned)
    char buf[20];
    calc_itoa(calc_value, buf);
    int len = 0;
    while (buf[len]) len++;
    int tx = x + w - 8 - len * FONT_WIDTH;
    int ty = y + (CALC_DISPLAY_H - FONT_HEIGHT) / 2;
    bb_draw_string(tx, ty, buf, COLOR_BLACK, 0x00E0FFE0);

    // Buttons
    int btn_y = y + CALC_DISPLAY_H + 10;
    for (int row = 0; row < 5; row++) {
        int btn_x = x;
        for (int col = 0; col < 4; col++) {
            int bw = CALC_BTN_W;

            if (row == 4) {
                // Last row: wide 0 (cols 0-1), wide = (cols 2-3)
                if (col == 0 || col == 2) {
                    bw = CALC_BTN_W * 2 + CALC_BTN_GAP;
                } else {
                    continue;  // Skip cols 1 and 3 (covered by wide buttons)
                }
            }

            draw_calc_button(btn_x, btn_y, bw, CALC_BTN_H, get_calc_btn(row, col), 0);
            btn_x += bw + CALC_BTN_GAP;
        }
        btn_y += CALC_BTN_H + CALC_BTN_GAP;
    }
}

static int calc_button_at(window_t *win, int mx, int my, int *row_out, int *col_out) {
    int x = win->x + 10;
    int y = win->y + TITLE_HEIGHT + 10 + CALC_DISPLAY_H + 10;

    for (int row = 0; row < 5; row++) {
        int btn_x = x;
        for (int col = 0; col < 4; col++) {
            int bw = CALC_BTN_W;

            if (row == 4) {
                // Last row: wide 0 (cols 0-1), wide = (cols 2-3)
                if (col == 0 || col == 2) {
                    bw = CALC_BTN_W * 2 + CALC_BTN_GAP;
                } else {
                    continue;  // Skip cols 1 and 3
                }
            }

            if (point_in_rect(mx, my, btn_x, y, bw, CALC_BTN_H)) {
                *row_out = row;
                *col_out = col;
                return 1;
            }
            btn_x += bw + CALC_BTN_GAP;
        }
        y += CALC_BTN_H + CALC_BTN_GAP;
    }
    return 0;
}

static void calc_handle_button(int row, int col) {
    const char *label = get_calc_btn(row, col);

    if (label[0] >= '0' && label[0] <= '9') {
        calc_digit(label[0] - '0');
    } else if (label[0] == 'C') {
        calc_clear();
    } else if (label[0] == '+' && label[1] == '/') {
        calc_value = -calc_value;
    } else if (label[0] == '%') {
        calc_value = calc_value / 100;
    } else if (label[0] == '+' || label[0] == '-' || label[0] == '*' || label[0] == '/') {
        calc_set_op(label[0]);
    } else if (label[0] == '=') {
        calc_equals();
    }
    // '.' ignored for now (integer calculator)
}

static void open_calculator(void) {
    // Check if already open
    if (calc_window >= 0 && windows[calc_window].visible) {
        focused_window = calc_window;
        return;
    }

    calc_clear();

    // Calculate window size based on button layout
    // Width: margin + 4 buttons + 3 gaps + margin
    int w = 10 + 4 * CALC_BTN_W + 3 * CALC_BTN_GAP + 10;
    // Height: just make it big enough - 250 should fit everything
    int h = 260;

    calc_window = create_window(100, 50, w, h, "Calculator", draw_calc_content);
}

// ============ File Explorer State ============

#define FILES_MAX_ENTRIES 32
#define FILES_ENTRY_HEIGHT 18
#define FILES_PATH_MAX 256

static int files_window = -1;
static char files_path[FILES_PATH_MAX] = "/";
static char files_entries[FILES_MAX_ENTRIES][64];
static uint8_t files_types[FILES_MAX_ENTRIES];  // 1=file, 2=dir
static int files_count = 0;
static int files_selected = -1;
static int files_scroll = 0;

// Context menu state
static int context_menu_open = 0;
static int context_menu_x = 0;
static int context_menu_y = 0;
#define CONTEXT_MENU_W 120
#define CONTEXT_MENU_ITEM_H 20
#define CONTEXT_MENU_ITEMS 4  // New File, New Folder, Rename, Delete

// Rename mode state
static int rename_mode = 0;
static int rename_index = -1;
static char rename_buf[64];
static int rename_cursor = 0;

// String copy helper
static void str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

// String compare helper
static int str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

// String length helper
static int str_len(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static void files_refresh(void) {
    files_count = 0;
    files_selected = -1;

    void *dir = api->open(files_path);
    if (!dir || !api->is_dir(dir)) {
        return;
    }

    // Read directory entries
    char name[64];
    uint8_t type;
    int i = 0;
    while (files_count < FILES_MAX_ENTRIES && api->readdir(dir, i, name, sizeof(name), &type) == 0) {
        str_copy(files_entries[files_count], name, 64);
        files_types[files_count] = type;
        files_count++;
        i++;
    }
}

static void files_navigate(const char *name) {
    if (str_equal(name, "..")) {
        // Go up one level
        if (str_equal(files_path, "/")) return;  // Already at root

        // Find last slash
        int last_slash = 0;
        for (int i = 0; files_path[i]; i++) {
            if (files_path[i] == '/') last_slash = i;
        }

        if (last_slash == 0) {
            // Going to root
            files_path[0] = '/';
            files_path[1] = '\0';
        } else {
            files_path[last_slash] = '\0';
        }
    } else {
        // Navigate into directory
        int path_len = str_len(files_path);
        if (path_len + 1 + str_len(name) < FILES_PATH_MAX - 1) {
            if (path_len > 1) {  // Not root
                files_path[path_len] = '/';
                str_copy(files_path + path_len + 1, name, FILES_PATH_MAX - path_len - 1);
            } else {
                files_path[1] = '\0';
                str_copy(files_path + 1, name, FILES_PATH_MAX - 1);
            }
        }
    }

    files_scroll = 0;
    files_refresh();
}

static void draw_files_content(window_t *win) {
    int x = win->x + 4;
    int y = win->y + TITLE_HEIGHT + 4;
    int w = win->w - 8;
    int content_h = win->h - TITLE_HEIGHT - 8;

    // Path bar background
    bb_fill_rect(x, y, w, 18, 0x00E0E0E0);
    bb_rect_outline(x, y, w, 18, COLOR_BLACK);

    // Truncate path to fit
    char display_path[40];
    int path_len = str_len(files_path);
    if (path_len > 35) {
        display_path[0] = '.';
        display_path[1] = '.';
        display_path[2] = '.';
        str_copy(display_path + 3, files_path + path_len - 32, 37);
    } else {
        str_copy(display_path, files_path, 40);
    }
    bb_draw_string(x + 4, y + 2, display_path, COLOR_BLACK, 0x00E0E0E0);

    // File list area
    int list_y = y + 22;
    int list_h = content_h - 26;
    bb_fill_rect(x, list_y, w, list_h, COLOR_WHITE);
    bb_rect_outline(x, list_y, w, list_h, COLOR_BLACK);

    // Draw ".." for going up (unless at root)
    int entry_y = list_y + 2;
    int visible_entries = (list_h - 4) / FILES_ENTRY_HEIGHT;

    if (!str_equal(files_path, "/")) {
        int is_selected = (files_selected == -2);  // -2 means ".." is selected
        uint32_t bg = is_selected ? 0x000066CC : COLOR_WHITE;
        uint32_t fg = is_selected ? COLOR_WHITE : COLOR_BLACK;
        bb_fill_rect(x + 2, entry_y, w - 4, FILES_ENTRY_HEIGHT, bg);
        bb_draw_string(x + 22, entry_y + 1, "..", fg, bg);

        // Folder icon for parent
        bb_fill_rect(x + 4, entry_y + 2, 14, 12, 0x00FFCC00);
        bb_rect_outline(x + 4, entry_y + 2, 14, 12, COLOR_BLACK);

        entry_y += FILES_ENTRY_HEIGHT;
        visible_entries--;
    }

    // Draw file entries
    for (int i = files_scroll; i < files_count && visible_entries > 0; i++) {
        int is_selected = (files_selected == i);
        int is_renaming = (rename_mode && rename_index == i);
        uint32_t bg = is_selected ? 0x000066CC : COLOR_WHITE;
        uint32_t fg = is_selected ? COLOR_WHITE : COLOR_BLACK;

        bb_fill_rect(x + 2, entry_y, w - 4, FILES_ENTRY_HEIGHT, bg);

        // Icon - folder or file
        if (files_types[i] == 2) {
            // Folder icon (mini)
            bb_fill_rect(x + 4, entry_y + 2, 14, 12, 0x00FFCC00);
            bb_rect_outline(x + 4, entry_y + 2, 14, 12, COLOR_BLACK);
        } else {
            // File icon (mini)
            bb_fill_rect(x + 4, entry_y + 2, 12, 14, COLOR_WHITE);
            bb_rect_outline(x + 4, entry_y + 2, 12, 14, COLOR_BLACK);
            bb_hline(x + 6, entry_y + 5, 8, 0x00808080);
            bb_hline(x + 6, entry_y + 8, 8, 0x00808080);
            bb_hline(x + 6, entry_y + 11, 6, 0x00808080);
        }

        if (is_renaming) {
            // Draw rename text box
            bb_fill_rect(x + 20, entry_y, w - 24, FILES_ENTRY_HEIGHT, COLOR_WHITE);
            bb_rect_outline(x + 20, entry_y, w - 24, FILES_ENTRY_HEIGHT, COLOR_BLACK);
            bb_draw_string(x + 22, entry_y + 1, rename_buf, COLOR_BLACK, COLOR_WHITE);
            // Draw cursor
            int cursor_x = x + 22 + rename_cursor * FONT_WIDTH;
            bb_vline(cursor_x, entry_y + 2, FILES_ENTRY_HEIGHT - 4, COLOR_BLACK);
        } else {
            // Truncate name if too long
            char display_name[28];
            int name_len = str_len(files_entries[i]);
            if (name_len > 25) {
                int j;
                for (j = 0; j < 22; j++) display_name[j] = files_entries[i][j];
                display_name[22] = '.';
                display_name[23] = '.';
                display_name[24] = '.';
                display_name[25] = '\0';
            } else {
                str_copy(display_name, files_entries[i], 28);
            }

            bb_draw_string(x + 22, entry_y + 1, display_name, fg, bg);
        }

        entry_y += FILES_ENTRY_HEIGHT;
        visible_entries--;
    }
}

static int files_entry_at(window_t *win, int mx, int my) {
    int x = win->x + 4;
    int y = win->y + TITLE_HEIGHT + 4 + 22;  // After path bar
    int w = win->w - 8;
    int content_h = win->h - TITLE_HEIGHT - 8 - 26;

    // Check if in list area
    if (!point_in_rect(mx, my, x, y, w, content_h)) {
        return -999;  // Not in list
    }

    int entry_y = y + 2;
    int idx = 0;

    // Check ".." entry
    if (!str_equal(files_path, "/")) {
        if (my >= entry_y && my < entry_y + FILES_ENTRY_HEIGHT) {
            return -2;  // ".." entry
        }
        entry_y += FILES_ENTRY_HEIGHT;
        idx = 0;
    }

    // Check file entries
    for (int i = files_scroll; i < files_count; i++) {
        if (my >= entry_y && my < entry_y + FILES_ENTRY_HEIGHT) {
            return i;
        }
        entry_y += FILES_ENTRY_HEIGHT;
    }

    return -999;  // No entry hit
}

static void open_files(void) {
    // Check if already open
    if (files_window >= 0 && windows[files_window].visible) {
        focused_window = files_window;
        return;
    }

    // Start at root
    str_copy(files_path, "/", FILES_PATH_MAX);
    files_scroll = 0;
    files_refresh();

    files_window = create_window(150, 60, 280, 320, "Files", draw_files_content);
}

// ============ Context Menu ============

static void draw_context_menu(void) {
    if (!context_menu_open) return;

    int x = context_menu_x;
    int y = context_menu_y;
    int h = CONTEXT_MENU_ITEM_H * CONTEXT_MENU_ITEMS + 4;

    // Make sure menu stays on screen
    if (x + CONTEXT_MENU_W > (int)screen_width) x = screen_width - CONTEXT_MENU_W - 2;
    if (y + h > (int)screen_height - DOCK_HEIGHT) y = screen_height - DOCK_HEIGHT - h - 2;

    // Shadow
    bb_fill_rect(x + 2, y + 2, CONTEXT_MENU_W, h, 0x00404040);

    // Background
    bb_fill_rect(x, y, CONTEXT_MENU_W, h, COLOR_WHITE);
    bb_rect_outline(x, y, CONTEXT_MENU_W, h, COLOR_BLACK);

    // Menu items
    int item_y = y + 2;
    uint32_t enabled = COLOR_BLACK;
    uint32_t disabled = 0x00808080;

    // New File
    bb_draw_string(x + 8, item_y + 2, "New File", enabled, COLOR_WHITE);
    item_y += CONTEXT_MENU_ITEM_H;

    // New Folder
    bb_draw_string(x + 8, item_y + 2, "New Folder", enabled, COLOR_WHITE);
    item_y += CONTEXT_MENU_ITEM_H;

    // Rename (only if something selected)
    bb_draw_string(x + 8, item_y + 2, "Rename", files_selected >= 0 ? enabled : disabled, COLOR_WHITE);
    item_y += CONTEXT_MENU_ITEM_H;

    // Delete (only if something selected)
    bb_draw_string(x + 8, item_y + 2, "Delete", files_selected >= 0 ? enabled : disabled, COLOR_WHITE);
}

static int context_menu_hit_test(int mx, int my) {
    if (!context_menu_open) return -1;

    int x = context_menu_x;
    int y = context_menu_y;
    int h = CONTEXT_MENU_ITEM_H * CONTEXT_MENU_ITEMS + 4;

    // Adjust position same as draw
    if (x + CONTEXT_MENU_W > (int)screen_width) x = screen_width - CONTEXT_MENU_W - 2;
    if (y + h > (int)screen_height - DOCK_HEIGHT) y = screen_height - DOCK_HEIGHT - h - 2;

    if (!point_in_rect(mx, my, x, y, CONTEXT_MENU_W, h)) {
        return -1;  // Outside menu
    }

    int rel_y = my - y - 2;
    int item = rel_y / CONTEXT_MENU_ITEM_H;
    if (item >= 0 && item < CONTEXT_MENU_ITEMS) {
        return item;
    }
    return -1;
}

// Generate unique filename
static int files_gen_name(char *buf, int bufsize, const char *prefix, const char *ext) {
    // Try "prefix" first, then "prefix 2", "prefix 3", etc.
    for (int n = 1; n < 100; n++) {
        if (n == 1) {
            str_copy(buf, prefix, bufsize);
            if (ext[0]) {
                int len = str_len(buf);
                buf[len] = '.';
                str_copy(buf + len + 1, ext, bufsize - len - 1);
            }
        } else {
            // Build "prefix N.ext"
            int i = 0;
            for (int j = 0; prefix[j] && i < bufsize - 10; j++) buf[i++] = prefix[j];
            buf[i++] = ' ';
            if (n >= 10) buf[i++] = '0' + (n / 10);
            buf[i++] = '0' + (n % 10);
            if (ext[0]) {
                buf[i++] = '.';
                for (int j = 0; ext[j] && i < bufsize - 1; j++) buf[i++] = ext[j];
            }
            buf[i] = '\0';
        }

        // Check if exists
        int exists = 0;
        for (int j = 0; j < files_count; j++) {
            if (str_equal(files_entries[j], buf)) {
                exists = 1;
                break;
            }
        }
        if (!exists) return 1;  // Found unique name
    }
    return 0;  // Failed
}

static void context_menu_action(int item) {
    char fullpath[FILES_PATH_MAX];
    char newname[64];

    if (item == 0) {
        // New File
        if (!files_gen_name(newname, 64, "Untitled", "txt")) return;

        // Build full path
        int plen = str_len(files_path);
        if (plen == 1) {
            fullpath[0] = '/';
            str_copy(fullpath + 1, newname, FILES_PATH_MAX - 1);
        } else {
            str_copy(fullpath, files_path, FILES_PATH_MAX);
            fullpath[plen] = '/';
            str_copy(fullpath + plen + 1, newname, FILES_PATH_MAX - plen - 1);
        }

        api->create(fullpath);
        files_refresh();
    } else if (item == 1) {
        // New Folder
        if (!files_gen_name(newname, 64, "New Folder", "")) return;

        int plen = str_len(files_path);
        if (plen == 1) {
            fullpath[0] = '/';
            str_copy(fullpath + 1, newname, FILES_PATH_MAX - 1);
        } else {
            str_copy(fullpath, files_path, FILES_PATH_MAX);
            fullpath[plen] = '/';
            str_copy(fullpath + plen + 1, newname, FILES_PATH_MAX - plen - 1);
        }

        api->mkdir(fullpath);
        files_refresh();
    } else if (item == 2 && files_selected >= 0) {
        // Rename - enter rename mode
        rename_mode = 1;
        rename_index = files_selected;
        str_copy(rename_buf, files_entries[files_selected], 64);
        rename_cursor = str_len(rename_buf);
    } else if (item == 3 && files_selected >= 0) {
        // Delete selected file
        int plen = str_len(files_path);
        if (plen == 1) {
            fullpath[0] = '/';
            str_copy(fullpath + 1, files_entries[files_selected], FILES_PATH_MAX - 1);
        } else {
            str_copy(fullpath, files_path, FILES_PATH_MAX);
            fullpath[plen] = '/';
            str_copy(fullpath + plen + 1, files_entries[files_selected], FILES_PATH_MAX - plen - 1);
        }

        api->delete(fullpath);
        files_refresh();
    }
}

// Commit rename operation
static void files_commit_rename(void) {
    if (!rename_mode || rename_index < 0) return;

    // Don't rename if name unchanged or empty
    if (rename_buf[0] == '\0' || str_equal(rename_buf, files_entries[rename_index])) {
        rename_mode = 0;
        return;
    }

    char oldpath[FILES_PATH_MAX];
    int plen = str_len(files_path);

    // Build old path
    if (plen == 1) {
        oldpath[0] = '/';
        str_copy(oldpath + 1, files_entries[rename_index], FILES_PATH_MAX - 1);
    } else {
        str_copy(oldpath, files_path, FILES_PATH_MAX);
        oldpath[plen] = '/';
        str_copy(oldpath + plen + 1, files_entries[rename_index], FILES_PATH_MAX - plen - 1);
    }

    // Use proper rename (works for both files and directories)
    api->rename(oldpath, rename_buf);

    rename_mode = 0;
    files_refresh();
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
    // Classic Mac menu bar - white with bottom border
    bb_fill_rect(0, 0, screen_width, MENU_HEIGHT, COLOR_WHITE);
    bb_hline(0, MENU_HEIGHT - 1, screen_width, COLOR_BLACK);

    // Draw apple icon (solid black apple)
    draw_apple_icon(6, 3);
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
    // Menu items with proper spacing (like classic Mac)
    bb_draw_string(24, 2, "File", COLOR_BLACK, COLOR_WHITE);
    bb_draw_string(64, 2, "Edit", COLOR_BLACK, COLOR_WHITE);
    bb_draw_string(104, 2, "View", COLOR_BLACK, COLOR_WHITE);
    bb_draw_string(152, 2, "Special", COLOR_BLACK, COLOR_WHITE);
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
    // Update dock hover state
    dock_hover = dock_hit_test(mouse_x, mouse_y);

    // Draw everything to backbuffer
    draw_desktop_pattern();
    draw_dock();
    draw_menu_bar();
    draw_menu_text();
    draw_all_windows_frames();
    draw_all_windows_text();
    draw_apple_dropdown();
    draw_context_menu();

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
        int right_clicked = (buttons & MOUSE_BTN_RIGHT) && !(prev_buttons & MOUSE_BTN_RIGHT);

        int needs_redraw = 0;

        // Handle right-click for context menu
        if (right_clicked) {
            // Only show context menu if in files window
            if (files_window >= 0 && windows[files_window].visible) {
                window_t *win = &windows[files_window];
                if (point_in_rect(new_mx, new_my, win->x, win->y + TITLE_HEIGHT, win->w, win->h - TITLE_HEIGHT)) {
                    context_menu_open = 1;
                    context_menu_x = new_mx;
                    context_menu_y = new_my;
                    needs_redraw = 1;
                }
            }
        }

        if (dragging) {
            if (released) {
                dragging = 0;
                drag_window = -1;
            } else {
                window_t *win = &windows[drag_window];
                win->x = new_mx - drag_offset_x;
                win->y = new_my - drag_offset_y;
                if (win->y < MENU_HEIGHT) win->y = MENU_HEIGHT;
                // Don't let window go below dock
                int max_y = (int)screen_height - DOCK_HEIGHT - win->h;
                if (win->y > max_y) win->y = max_y;
                needs_redraw = 1;
            }
        } else if (clicked) {
            // Check if clicking on context menu first
            if (context_menu_open) {
                int item = context_menu_hit_test(new_mx, new_my);
                if (item >= 0) {
                    context_menu_action(item);
                }
                context_menu_open = 0;
                needs_redraw = 1;
            }
            // Check if clicking on dock
            else if (dock_hit_test(new_mx, new_my) >= 0) {
                int dock_app = dock_hit_test(new_mx, new_my);
                if (dock_app == DOCK_APP_CALC) {
                    open_calculator();
                } else if (dock_app == DOCK_APP_FILES) {
                    open_files();
                }
                needs_redraw = 1;
            }
            // Check if clicking on apple icon in menu bar
            else if (point_in_rect(new_mx, new_my, APPLE_MENU_X, 0, APPLE_MENU_W, MENU_HEIGHT)) {
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
                        // If closing calculator, reset calc_window
                        if (win_idx == calc_window) {
                            calc_window = -1;
                        }
                        // If closing file explorer, reset files_window
                        if (win_idx == files_window) {
                            files_window = -1;
                        }
                        close_window(win_idx);
                        needs_redraw = 1;
                    } else if (point_in_rect(new_mx, new_my, win->x, win->y, win->w, TITLE_HEIGHT)) {
                        dragging = 1;
                        drag_window = win_idx;
                        drag_offset_x = new_mx - win->x;
                        drag_offset_y = new_my - win->y;
                        focused_window = win_idx;
                        needs_redraw = 1;
                    } else {
                        // Clicked inside window content area
                        if (win_idx != focused_window) {
                            focused_window = win_idx;
                            needs_redraw = 1;
                        }

                        // Handle calculator button clicks
                        if (win_idx == calc_window) {
                            int row, col;
                            if (calc_button_at(win, new_mx, new_my, &row, &col)) {
                                calc_handle_button(row, col);
                                needs_redraw = 1;
                            }
                        }

                        // Handle file explorer clicks
                        if (win_idx == files_window) {
                            int entry = files_entry_at(win, new_mx, new_my);
                            if (entry == -2) {
                                // Clicked ".." - go up
                                files_navigate("..");
                                needs_redraw = 1;
                            } else if (entry >= 0 && entry < files_count) {
                                // Clicked an entry
                                if (files_selected == entry) {
                                    // Double-click (second click on same item)
                                    if (files_types[entry] == 2) {
                                        // Navigate into directory
                                        files_navigate(files_entries[entry]);
                                    }
                                    // For files, we could open them later
                                } else {
                                    // First click - select
                                    files_selected = entry;
                                }
                                needs_redraw = 1;
                            }
                        }
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

        // Handle keyboard input
        while (api->has_key()) {
            int c = api->getc();

            if (rename_mode) {
                if (c == '\n' || c == '\r') {
                    // Enter - commit rename
                    files_commit_rename();
                    needs_redraw = 1;
                } else if (c == 27) {
                    // Escape - cancel rename
                    rename_mode = 0;
                    needs_redraw = 1;
                } else if (c == '\b' || c == 127) {
                    // Backspace
                    if (rename_cursor > 0) {
                        rename_cursor--;
                        // Shift characters left
                        for (int i = rename_cursor; rename_buf[i]; i++) {
                            rename_buf[i] = rename_buf[i + 1];
                        }
                        needs_redraw = 1;
                    }
                } else if (c >= 32 && c < 127) {
                    // Printable character
                    int len = str_len(rename_buf);
                    if (len < 60) {
                        // Shift characters right
                        for (int i = len + 1; i > rename_cursor; i--) {
                            rename_buf[i] = rename_buf[i - 1];
                        }
                        rename_buf[rename_cursor] = c;
                        rename_cursor++;
                        needs_redraw = 1;
                    }
                }
            }
        }

        // Redraw if needed
        if (needs_redraw) {
            redraw_all(new_mx, new_my);
        }

        delay(5000);
    }

    // Cleanup
    api->free(backbuffer);
    api->clear();
    api->puts("Desktop exited.\n");

    return 0;
}

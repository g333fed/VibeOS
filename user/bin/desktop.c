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

// ============ Dock ============

#define DOCK_APP_CALC 0
#define DOCK_APP_COUNT 1

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

    // Label under icon (centered)
    bb_draw_string(start_x, icon_y + DOCK_ICON_SIZE + 2, "Calc", COLOR_BLACK, 0x00C0C0C0);
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
                // Don't let window go below dock
                int max_y = (int)screen_height - DOCK_HEIGHT - win->h;
                if (win->y > max_y) win->y = max_y;
                needs_redraw = 1;
            }
        } else if (clicked) {
            // Check if clicking on dock
            int dock_app = dock_hit_test(new_mx, new_my);
            if (dock_app >= 0) {
                if (dock_app == DOCK_APP_CALC) {
                    open_calculator();
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

/*
 * VibeOS Calculator
 *
 * Simple calculator that runs in a desktop window.
 * Uses the window API to create and manage its window.
 */

#include "vibe.h"

static kapi_t *api;
static int window_id = -1;
static uint32_t *win_buffer;
static int win_w, win_h;

// Calculator state
static int display_value = 0;
static int pending_value = 0;
static char pending_op = 0;
static int clear_on_digit = 0;

// Button layout
#define BTN_W 40
#define BTN_H 30
#define BTN_PAD 4
#define DISPLAY_H 30

static const char button_labels[4][4][3] = {
    { "7", "8", "9", "/" },
    { "4", "5", "6", "*" },
    { "1", "2", "3", "-" },
    { "C", "0", "=", "+" }
};

// ============ Drawing Helpers ============

static void buf_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int py = y; py < y + h && py < win_h; py++) {
        if (py < 0) continue;
        for (int px = x; px < x + w && px < win_w; px++) {
            if (px < 0) continue;
            win_buffer[py * win_w + px] = color;
        }
    }
}

static void buf_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    const uint8_t *glyph = &api->font_data[(unsigned char)c * 16];
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            uint32_t color = (glyph[row] & (0x80 >> col)) ? fg : bg;
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < win_w && py >= 0 && py < win_h) {
                win_buffer[py * win_w + px] = color;
            }
        }
    }
}

static void buf_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg) {
    while (*s) {
        buf_draw_char(x, y, *s, fg, bg);
        x += 8;
        s++;
    }
}

static void buf_draw_rect(int x, int y, int w, int h, uint32_t color) {
    // Horizontal lines
    for (int i = 0; i < w; i++) {
        if (x + i >= 0 && x + i < win_w) {
            if (y >= 0 && y < win_h) win_buffer[y * win_w + x + i] = color;
            if (y + h - 1 >= 0 && y + h - 1 < win_h) win_buffer[(y + h - 1) * win_w + x + i] = color;
        }
    }
    // Vertical lines
    for (int i = 0; i < h; i++) {
        if (y + i >= 0 && y + i < win_h) {
            if (x >= 0 && x < win_w) win_buffer[(y + i) * win_w + x] = color;
            if (x + w - 1 >= 0 && x + w - 1 < win_w) win_buffer[(y + i) * win_w + x + w - 1] = color;
        }
    }
}

// ============ Drawing ============

static void draw_display(void) {
    // Display background
    buf_fill_rect(BTN_PAD, BTN_PAD, win_w - BTN_PAD * 2, DISPLAY_H, 0x00EEEEEE);
    buf_draw_rect(BTN_PAD, BTN_PAD, win_w - BTN_PAD * 2, DISPLAY_H, COLOR_BLACK);

    // Format number
    char buf[16];
    int n = display_value;
    int neg = 0;
    if (n < 0) {
        neg = 1;
        n = -n;
    }

    int i = 15;
    buf[i--] = '\0';
    if (n == 0) {
        buf[i--] = '0';
    } else {
        while (n > 0 && i >= 0) {
            buf[i--] = '0' + (n % 10);
            n /= 10;
        }
    }
    if (neg && i >= 0) {
        buf[i--] = '-';
    }
    i++;

    // Right-align in display
    int text_len = strlen(&buf[i]);
    int text_x = win_w - BTN_PAD * 2 - text_len * 8 - 4;
    buf_draw_string(text_x, BTN_PAD + 8, &buf[i], COLOR_BLACK, 0x00EEEEEE);
}

static void draw_button(int row, int col, int pressed) {
    int x = BTN_PAD + col * (BTN_W + BTN_PAD);
    int y = DISPLAY_H + BTN_PAD * 2 + row * (BTN_H + BTN_PAD);

    uint32_t bg = pressed ? 0x00888888 : 0x00CCCCCC;
    uint32_t fg = COLOR_BLACK;

    // Button face
    buf_fill_rect(x, y, BTN_W, BTN_H, bg);
    buf_draw_rect(x, y, BTN_W, BTN_H, COLOR_BLACK);

    // 3D effect
    if (!pressed) {
        // Top and left highlight
        for (int i = 0; i < BTN_W - 1; i++) {
            if (x + 1 + i < win_w && y + 1 < win_h)
                win_buffer[(y + 1) * win_w + x + 1 + i] = COLOR_WHITE;
        }
        for (int i = 0; i < BTN_H - 1; i++) {
            if (x + 1 < win_w && y + 1 + i < win_h)
                win_buffer[(y + 1 + i) * win_w + x + 1] = COLOR_WHITE;
        }
    }

    // Label
    const char *label = button_labels[row][col];
    int label_len = strlen(label);
    int lx = x + (BTN_W - label_len * 8) / 2;
    int ly = y + (BTN_H - 16) / 2;
    buf_draw_string(lx, ly, label, fg, bg);
}

static void draw_all(void) {
    // Clear background
    buf_fill_rect(0, 0, win_w, win_h, 0x00DDDDDD);

    // Display
    draw_display();

    // Buttons
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            draw_button(row, col, 0);
        }
    }

    api->window_invalidate(window_id);
}

// ============ Calculator Logic ============

static void do_op(void) {
    switch (pending_op) {
        case '+': display_value = pending_value + display_value; break;
        case '-': display_value = pending_value - display_value; break;
        case '*': display_value = pending_value * display_value; break;
        case '/':
            if (display_value != 0)
                display_value = pending_value / display_value;
            break;
    }
    pending_op = 0;
}

static void button_click(int row, int col) {
    const char *label = button_labels[row][col];
    char c = label[0];

    if (c >= '0' && c <= '9') {
        int digit = c - '0';
        if (clear_on_digit) {
            display_value = digit;
            clear_on_digit = 0;
        } else {
            display_value = display_value * 10 + digit;
        }
    } else if (c == 'C') {
        display_value = 0;
        pending_value = 0;
        pending_op = 0;
        clear_on_digit = 0;
    } else if (c == '=') {
        if (pending_op) {
            do_op();
        }
        clear_on_digit = 1;
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        if (pending_op) {
            do_op();
        }
        pending_value = display_value;
        pending_op = c;
        clear_on_digit = 1;
    }
}

// ============ Input Handling ============

static int button_at_point(int x, int y) {
    // Returns button index (row * 4 + col) or -1
    int bx = x - BTN_PAD;
    int by = y - (DISPLAY_H + BTN_PAD * 2);

    if (bx < 0 || by < 0) return -1;

    int col = bx / (BTN_W + BTN_PAD);
    int row = by / (BTN_H + BTN_PAD);

    if (row < 0 || row >= 4 || col < 0 || col >= 4) return -1;

    // Check if actually within button bounds (not in padding)
    int btn_x = BTN_PAD + col * (BTN_W + BTN_PAD);
    int btn_y = DISPLAY_H + BTN_PAD * 2 + row * (BTN_H + BTN_PAD);
    if (x < btn_x || x >= btn_x + BTN_W) return -1;
    if (y < btn_y || y >= btn_y + BTN_H) return -1;

    return row * 4 + col;
}

// ============ Main ============

int main(kapi_t *kapi, int argc, char **argv) {
    (void)argc;
    (void)argv;

    api = kapi;

    // Wait for window API to be available (desktop must be running)
    if (!api->window_create) {
        api->puts("calc: window API not available (desktop not running?)\n");
        return 1;
    }

    // Calculate window size
    int content_w = BTN_PAD * 2 + 4 * BTN_W + 3 * BTN_PAD;
    int content_h = DISPLAY_H + BTN_PAD * 3 + 4 * BTN_H + 3 * BTN_PAD;

    // Create window (add title bar height in desktop)
    window_id = api->window_create(200, 100, content_w, content_h + 18, "Calculator");
    if (window_id < 0) {
        api->puts("calc: failed to create window\n");
        return 1;
    }

    // Get buffer
    win_buffer = api->window_get_buffer(window_id, &win_w, &win_h);
    if (!win_buffer) {
        api->puts("calc: failed to get window buffer\n");
        api->window_destroy(window_id);
        return 1;
    }

    // Initial draw
    draw_all();

    // Event loop
    int running = 1;
    while (running) {
        int event_type, data1, data2, data3;
        while (api->window_poll_event(window_id, &event_type, &data1, &data2, &data3)) {
            switch (event_type) {
                case WIN_EVENT_CLOSE:
                    running = 0;
                    break;

                case WIN_EVENT_MOUSE_DOWN: {
                    int btn = button_at_point(data1, data2);
                    if (btn >= 0) {
                        int row = btn / 4;
                        int col = btn % 4;
                        draw_button(row, col, 1);  // Pressed
                        api->window_invalidate(window_id);
                    }
                    break;
                }

                case WIN_EVENT_MOUSE_UP: {
                    int btn = button_at_point(data1, data2);
                    if (btn >= 0) {
                        int row = btn / 4;
                        int col = btn % 4;
                        button_click(row, col);
                    }
                    draw_all();  // Redraw with button unpressed
                    break;
                }

                case WIN_EVENT_KEY: {
                    char c = (char)data1;
                    // Map keys to buttons
                    if (c >= '0' && c <= '9') {
                        int digit = c - '0';
                        int row = (9 - digit) / 3;
                        int col = (digit - 1) % 3;
                        if (digit == 0) { row = 3; col = 1; }
                        button_click(row, col);
                        draw_all();
                    } else if (c == '+') { button_click(3, 3); draw_all(); }
                    else if (c == '-') { button_click(2, 3); draw_all(); }
                    else if (c == '*') { button_click(1, 3); draw_all(); }
                    else if (c == '/') { button_click(0, 3); draw_all(); }
                    else if (c == '=' || c == '\r' || c == '\n') { button_click(3, 2); draw_all(); }
                    else if (c == 'c' || c == 'C') { button_click(3, 0); draw_all(); }
                    else if (c == 'q' || c == 'Q') { running = 0; }
                    break;
                }
            }
        }

        // Yield to other processes
        api->yield();
    }

    api->window_destroy(window_id);
    return 0;
}

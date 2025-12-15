// Stubs for MicroPython on VibeOS

#include <stddef.h>
#include "py/mphal.h"

// strchr needs to be a real function (not just inline) because objstr.c uses it
// in conditional expressions that need its address
char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : (void *)0;
}

// Keyboard interrupt is disabled in minimal config, but mphalport.c calls this
void mp_sched_keyboard_interrupt(void) {
    // Just ignore Ctrl+C for now - no scheduler in minimal config
}

// VT100 stubs - VibeOS console doesn't support escape codes
// Use simple backspace characters instead
void mp_hal_move_cursor_back(unsigned int pos) {
    // Output backspace characters to move cursor back
    while (pos--) {
        mp_hal_stdout_tx_strn("\b", 1);
    }
}

void mp_hal_erase_line_from_cursor(unsigned int n_chars) {
    // Erase by printing spaces then moving back
    for (unsigned int i = 0; i < n_chars; i++) {
        mp_hal_stdout_tx_strn(" ", 1);
    }
    mp_hal_move_cursor_back(n_chars);
}

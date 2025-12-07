/*
 * VibeOS Userspace Library
 *
 * Programs receive a pointer to kernel API and call functions directly.
 * No syscalls needed - Win3.1 style!
 */

#ifndef _VIBE_H
#define _VIBE_H

typedef unsigned long size_t;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

// Kernel API structure (must match kernel/kapi.h)
typedef struct kapi {
    uint32_t version;

    // Console I/O
    void (*putc)(char c);
    void (*puts)(const char *s);
    int  (*getc)(void);
    void (*set_color)(uint32_t fg, uint32_t bg);
    void (*clear)(void);
    void (*set_cursor)(int row, int col);
    void (*print_int)(int n);
    void (*print_hex)(uint32_t n);

    // Keyboard
    int  (*has_key)(void);

    // Memory
    void *(*malloc)(size_t size);
    void  (*free)(void *ptr);

    // Filesystem
    void *(*open)(const char *path);
    int   (*read)(void *file, char *buf, size_t size, size_t offset);
    int   (*write)(void *file, const char *buf, size_t size);
    int   (*is_dir)(void *node);
    void *(*create)(const char *path);
    void *(*mkdir)(const char *path);

    // Process
    void (*exit)(int status);
    int  (*exec)(const char *path);
} kapi_t;

// Colors (must match kernel fb.h - these are RGB values)
#define COLOR_BLACK   0x00000000
#define COLOR_WHITE   0x00FFFFFF
#define COLOR_RED     0x00FF0000
#define COLOR_GREEN   0x0000FF00
#define COLOR_BLUE    0x000000FF
#define COLOR_CYAN    0x0000FFFF
#define COLOR_MAGENTA 0x00FF00FF
#define COLOR_YELLOW  0x00FFFF00
#define COLOR_AMBER   0x00FFBF00

// String length
static inline size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

#endif

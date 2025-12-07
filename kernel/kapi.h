/*
 * VibeOS Kernel API
 *
 * Function pointers passed to userspace programs.
 * Programs call kernel functions directly - no syscalls needed.
 * Win3.1 style!
 */

#ifndef KAPI_H
#define KAPI_H

#include <stdint.h>
#include <stddef.h>

// Kernel API version
#define KAPI_VERSION 1

// The kernel API structure - passed to every program
typedef struct {
    uint32_t version;

    // Console I/O
    void (*putc)(char c);
    void (*puts)(const char *s);
    int  (*getc)(void);              // Non-blocking, returns -1 if no input
    void (*set_color)(uint32_t fg, uint32_t bg);
    void (*clear)(void);             // Clear screen
    void (*set_cursor)(int row, int col);  // Set cursor position
    void (*print_int)(int n);        // Print integer
    void (*print_hex)(uint32_t n);   // Print hex

    // Keyboard
    int  (*has_key)(void);           // Check if key available

    // Memory
    void *(*malloc)(size_t size);
    void  (*free)(void *ptr);

    // Filesystem
    void *(*open)(const char *path);  // Returns vfs_node_t*
    int   (*read)(void *file, char *buf, size_t size, size_t offset);
    int   (*write)(void *file, const char *buf, size_t size);
    int   (*is_dir)(void *node);
    void *(*create)(const char *path);
    void *(*mkdir)(const char *path);

    // Process
    void (*exit)(int status);
    int  (*exec)(const char *path);   // Run another program

} kapi_t;

// Global kernel API instance
extern kapi_t kapi;

// Initialize the kernel API
void kapi_init(void);

#endif

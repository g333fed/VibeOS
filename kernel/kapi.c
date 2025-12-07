/*
 * VibeOS Kernel API Implementation
 */

#include "kapi.h"
#include "console.h"
#include "keyboard.h"
#include "memory.h"
#include "vfs.h"
#include "process.h"

// Global kernel API instance
kapi_t kapi;

// Wrapper for exit (needs to match signature)
static void kapi_exit(int status) {
    process_exit(status);
}

// Print integer (simple implementation)
static void kapi_print_int(int n) {
    if (n < 0) {
        console_putc('-');
        n = -n;
    }
    if (n == 0) {
        console_putc('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) {
        console_putc(buf[--i]);
    }
}

// Print hex
static void kapi_print_hex(uint32_t n) {
    const char *hex = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        console_putc(hex[(n >> (i * 4)) & 0xF]);
    }
}

// Wrapper for exec
static int kapi_exec(const char *path) {
    return process_exec(path);
}

// Wrapper for console color
static void kapi_set_color(uint32_t fg, uint32_t bg) {
    console_set_color(fg, bg);
}

// Wrapper for VFS open
static void *kapi_open(const char *path) {
    return (void *)vfs_lookup(path);
}

// Wrapper for VFS read
static int kapi_read(void *file, char *buf, size_t size, size_t offset) {
    return vfs_read((vfs_node_t *)file, buf, size, offset);
}

// Wrapper for VFS write
static int kapi_write(void *file, const char *buf, size_t size) {
    return vfs_write((vfs_node_t *)file, buf, size);
}

// Wrapper for is_dir
static int kapi_is_dir(void *node) {
    return vfs_is_dir((vfs_node_t *)node);
}

// Wrapper for create
static void *kapi_create(const char *path) {
    return (void *)vfs_create(path);
}

// Wrapper for mkdir
static void *kapi_mkdir(const char *path) {
    return (void *)vfs_mkdir(path);
}

void kapi_init(void) {
    kapi.version = KAPI_VERSION;

    // Console
    kapi.putc = console_putc;
    kapi.puts = console_puts;
    kapi.getc = keyboard_getc;
    kapi.set_color = kapi_set_color;
    kapi.clear = console_clear;
    kapi.set_cursor = console_set_cursor;
    kapi.print_int = kapi_print_int;
    kapi.print_hex = kapi_print_hex;

    // Keyboard
    kapi.has_key = keyboard_has_key;

    // Memory
    kapi.malloc = malloc;
    kapi.free = free;

    // Filesystem
    kapi.open = kapi_open;
    kapi.read = kapi_read;
    kapi.write = kapi_write;
    kapi.is_dir = kapi_is_dir;
    kapi.create = kapi_create;
    kapi.mkdir = kapi_mkdir;

    // Process
    kapi.exit = kapi_exit;
    kapi.exec = kapi_exec;
}

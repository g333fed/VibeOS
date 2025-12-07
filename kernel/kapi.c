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

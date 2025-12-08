/*
 * ls - list directory contents
 *
 * Uses the proper readdir API instead of accessing VFS internals.
 */

#include "../lib/vibe.h"

static kapi_t *api;

// I/O helpers - use stdio hooks if available
static void out_putc(char c) {
    if (api->stdio_putc) api->stdio_putc(c);
    else api->putc(c);
}

static void out_puts(const char *s) {
    if (api->stdio_puts) api->stdio_puts(s);
    else api->puts(s);
}

int main(kapi_t *k, int argc, char **argv) {
    api = k;
    const char *path = ".";

    if (argc > 1) {
        path = argv[1];
    }

    void *dir = k->open(path);
    if (!dir) {
        out_puts("ls: ");
        out_puts(path);
        out_puts(": No such file or directory\n");
        return 1;
    }

    if (!k->is_dir(dir)) {
        // It's a file, just print the name
        out_puts(path);
        out_putc('\n');
        return 0;
    }

    // List directory contents using readdir
    char name[256];
    uint8_t type;
    int index = 0;

    while (k->readdir(dir, index, name, sizeof(name), &type) >= 0) {
        out_puts(name);
        if (type == 2) {
            // Directory
            out_putc('/');
        }
        out_putc('\n');
        index++;
    }

    return 0;
}

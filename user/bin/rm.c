/*
 * rm - remove files and directories
 */

#include "../lib/vibe.h"

static kapi_t *api;

static void out_puts(const char *s) {
    if (api->stdio_puts) api->stdio_puts(s);
    else api->puts(s);
}

int main(kapi_t *k, int argc, char **argv) {
    api = k;

    if (argc < 2) {
        out_puts("Usage: rm [-r] <file> [...]\n");
        return 1;
    }

    int status = 0;
    int recursive = 0;
    int start_idx = 1;

    // Check for -r flag
    if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == 'r' && argv[1][2] == '\0') {
        recursive = 1;
        start_idx = 2;
    }

    if (start_idx >= argc) {
        out_puts("Usage: rm [-r] <file> [...]\n");
        return 1;
    }

    for (int i = start_idx; i < argc; i++) {
        int result;
        if (recursive) {
            result = k->delete_recursive(argv[i]);
        } else {
            result = k->delete(argv[i]);
        }

        if (result < 0) {
            out_puts("rm: cannot remove '");
            out_puts(argv[i]);
            out_puts("'");
            if (!recursive) {
                out_puts(" (directory? use -r)");
            }
            out_puts("\n");
            status = 1;
        }
    }

    return status;
}

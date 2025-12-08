/*
 * rm - remove files
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
        out_puts("Usage: rm <file> [...]\n");
        return 1;
    }

    int status = 0;

    for (int i = 1; i < argc; i++) {
        if (k->delete(argv[i]) < 0) {
            out_puts("rm: cannot remove '");
            out_puts(argv[i]);
            out_puts("'\n");
            status = 1;
        }
    }

    return status;
}

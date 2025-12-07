/*
 * touch - create empty file
 */

#include "../lib/vibe.h"

int main(kapi_t *k, int argc, char **argv) {
    if (argc < 2) {
        k->puts("usage: touch <file>\n");
        return 1;
    }

    void *file = k->create(argv[1]);
    if (!file) {
        k->puts("touch: cannot create '");
        k->puts(argv[1]);
        k->puts("'\n");
        return 1;
    }

    return 0;
}

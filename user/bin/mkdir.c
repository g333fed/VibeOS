/*
 * mkdir - create directory
 */

#include "../lib/vibe.h"

int main(kapi_t *k, int argc, char **argv) {
    if (argc < 2) {
        k->puts("usage: mkdir <directory>\n");
        return 1;
    }

    void *dir = k->mkdir(argv[1]);
    if (!dir) {
        k->puts("mkdir: cannot create directory '");
        k->puts(argv[1]);
        k->puts("'\n");
        return 1;
    }

    return 0;
}

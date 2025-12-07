/*
 * echo - print arguments
 */

#include "../lib/vibe.h"

int main(kapi_t *k, int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        k->puts(argv[i]);
        if (i < argc - 1) {
            k->putc(' ');
        }
    }
    k->putc('\n');
    return 0;
}

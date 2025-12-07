/*
 * rm - remove file
 */

#include "../lib/vibe.h"

int main(kapi_t *k, int argc, char **argv) {
    if (argc < 2) {
        k->puts("usage: rm <file>\n");
        return 1;
    }

    // TODO: Need to add remove to kapi
    k->puts("rm: not yet implemented\n");
    return 1;
}

/*
 * cat - print file contents
 */

#include "../lib/vibe.h"

int main(kapi_t *k, int argc, char **argv) {
    if (argc < 2) {
        k->puts("usage: cat <file>\n");
        return 1;
    }

    void *file = k->open(argv[1]);
    if (!file) {
        k->puts("cat: file not found: ");
        k->puts(argv[1]);
        k->putc('\n');
        return 1;
    }

    if (k->is_dir(file)) {
        k->puts("cat: is a directory: ");
        k->puts(argv[1]);
        k->putc('\n');
        return 1;
    }

    char buf[256];
    size_t offset = 0;
    int bytes;

    while ((bytes = k->read(file, buf, sizeof(buf) - 1, offset)) > 0) {
        buf[bytes] = '\0';
        k->puts(buf);
        offset += bytes;
    }

    return 0;
}

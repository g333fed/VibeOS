/*
 * play - Play audio files
 *
 * Usage: play <file.wav>
 */

#include "../lib/vibe.h"

static kapi_t *api;

// Output helpers that use stdio hooks if available
static void out_putc(char c) {
    if (api->stdio_putc) api->stdio_putc(c);
    else api->putc(c);
}

static void out_puts(const char *s) {
    if (api->stdio_puts) api->stdio_puts(s);
    else api->puts(s);
}

static void out_int(int n) {
    if (n < 0) {
        out_putc('-');
        n = -n;
    }
    if (n == 0) {
        out_putc('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) {
        out_putc(buf[--i]);
    }
}


int main(kapi_t *k, int argc, char **argv) {
    api = k;

    if (argc < 2) {
        out_puts("Usage: play <file.wav>\n");
        return 1;
    }

    const char *filename = argv[1];

    // Check if sound is available
    if (!api->sound_play_wav) {
        out_puts("Error: Sound not available\n");
        return 1;
    }

    // Open the file
    void *file = api->open(filename);
    if (!file) {
        out_puts("Error: Cannot open ");
        out_puts(filename);
        out_puts("\n");
        return 1;
    }

    if (api->is_dir(file)) {
        out_puts("Error: ");
        out_puts(filename);
        out_puts(" is a directory\n");
        return 1;
    }

    // Get file size
    int size = api->file_size(file);
    if (size <= 0) {
        out_puts("Error: Empty or invalid file\n");
        return 1;
    }

    out_puts("Loading ");
    out_puts(filename);
    out_puts(" (");
    out_int(size);
    out_puts(" bytes)...\n");

    // Allocate buffer
    char *data = api->malloc(size);
    if (!data) {
        out_puts("Error: Out of memory\n");
        return 1;
    }

    // Read file
    int offset = 0;
    while (offset < size) {
        int n = api->read(file, data + offset, size - offset, offset);
        if (n <= 0) break;
        offset += n;
    }

    if (offset != size) {
        out_puts("Warning: Only read ");
        out_int(offset);
        out_puts(" bytes\n");
    }

    out_puts("Playing...\n");

    // Play the WAV file
    int result = api->sound_play_wav(data, size);

    api->free(data);

    if (result < 0) {
        out_puts("Error: Playback failed\n");
        return 1;
    }

    out_puts("Done!\n");
    return 0;
}

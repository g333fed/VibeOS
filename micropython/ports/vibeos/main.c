// MicroPython for VibeOS
// Entry point and runtime initialization

#include "vibe.h"
#include "py/builtin.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/stackctrl.h"
#include "py/lexer.h"
#include "py/parse.h"
#include "py/nlr.h"
#include "shared/runtime/pyexec.h"

// Global kernel API pointer (used by mphalport.c)
kapi_t *mp_vibeos_api;

// Heap for MicroPython's garbage collector
// Must be aligned for pointer-sized access (GC stores pointers in heap)
static char heap[MICROPY_HEAP_SIZE] __attribute__((aligned(16)));

// Stack tracking
static char *stack_top;

// GC collection - scan stack for roots
void gc_collect(void) {
    void *dummy;
    gc_collect_start();
    // Sanity check: stack grows down on ARM64, so stack_top should be > &dummy
    // Also limit scan to reasonable size (1MB max) to prevent corruption issues
    mp_uint_t top = (mp_uint_t)stack_top;
    mp_uint_t cur = (mp_uint_t)&dummy;
    if (top > cur && (top - cur) < (1024 * 1024)) {
        gc_collect_root(&dummy, (top - cur) / sizeof(mp_uint_t));
    }
    gc_collect_end();
}

// File operations for script execution
mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
    const char *path = qstr_str(filename);

    // Open the file
    void *file = mp_vibeos_api->open(path);
    if (!file) {
        mp_raise_OSError(MP_ENOENT);
    }

    // Check if it's a directory
    if (mp_vibeos_api->is_dir(file)) {
        mp_raise_OSError(MP_EISDIR);
    }

    // Get file size
    int size = mp_vibeos_api->file_size(file);
    if (size < 0) {
        mp_raise_OSError(MP_EIO);
    }

    // Allocate buffer using kernel malloc (simpler, small leak is OK for scripts)
    char *buf = mp_vibeos_api->malloc(size + 1);
    if (!buf) {
        mp_raise_OSError(MP_ENOMEM);
    }

    // Read file contents
    int bytes_read = mp_vibeos_api->read(file, buf, size, 0);
    if (bytes_read != size) {
        mp_vibeos_api->free(buf);
        mp_raise_OSError(MP_EIO);
    }

    // Strip \r (CRLF -> LF) - FAT32 files from macOS/Windows have CRLF
    int j = 0;
    for (int k = 0; k < size; k++) {
        if (buf[k] != '\r') {
            buf[j++] = buf[k];
        }
    }
    size = j;
    buf[size] = '\0';

    // Debug: show first 32 bytes as hex to detect BOM/weird chars
    mp_vibeos_api->uart_puts("DEBUG: size=");
    char hex[8];
    const char *hexchars = "0123456789ABCDEF";
    int show = size < 32 ? size : 32;
    for (int x = 0; x < 4; x++) {
        hex[x] = hexchars[(size >> (12 - x*4)) & 0xF];
    }
    hex[4] = '\0';
    mp_vibeos_api->uart_puts(hex);
    mp_vibeos_api->uart_puts(" hex=[");
    for (int x = 0; x < show; x++) {
        unsigned char c = buf[x];
        hex[0] = hexchars[(c >> 4) & 0xF];
        hex[1] = hexchars[c & 0xF];
        hex[2] = ' ';
        hex[3] = '\0';
        mp_vibeos_api->uart_puts(hex);
    }
    mp_vibeos_api->uart_puts("]\n");

    // Create lexer from buffer (pass 0 for free_len - we won't free it)
    return mp_lexer_new_from_str_len(filename, buf, size, 0);
}

mp_import_stat_t mp_import_stat(const char *path) {
    void *file = mp_vibeos_api->open(path);
    if (!file) {
        return MP_IMPORT_STAT_NO_EXIST;
    }
    if (mp_vibeos_api->is_dir(file)) {
        return MP_IMPORT_STAT_DIR;
    }
    return MP_IMPORT_STAT_FILE;
}

void nlr_jump_fail(void *val) {
    mp_vibeos_api->puts("FATAL: nlr_jump_fail\n");
    mp_vibeos_api->exit(1);
    for (;;) {}
}

void NORETURN __fatal_error(const char *msg) {
    mp_vibeos_api->puts("FATAL: ");
    mp_vibeos_api->puts(msg);
    mp_vibeos_api->puts("\n");
    mp_vibeos_api->exit(1);
    for (;;) {}
}

#ifndef NDEBUG
void __assert_func(const char *file, int line, const char *func, const char *expr) {
    mp_vibeos_api->puts("Assertion failed: ");
    mp_vibeos_api->puts(expr);
    mp_vibeos_api->puts("\n");
    __fatal_error("assertion failed");
}
#endif

int main(kapi_t *api, int argc, char **argv) {
    mp_vibeos_api = api;

    // Track stack for GC - capture SP at very start of main
    // Use inline asm to get actual stack pointer value
    char *sp_val;
    asm volatile("mov %0, sp" : "=r" (sp_val));
    stack_top = sp_val;

    // Initialize MicroPython
    mp_stack_ctrl_init();
    mp_stack_set_limit(64 * 1024);  // 64KB stack limit

    gc_init(heap, heap + sizeof(heap));
    mp_init();

    int ret = 0;

    if (argc > 1) {
        // Run script file - read and execute directly
        api->uart_puts("[MP] Opening: ");
        api->uart_puts(argv[1]);
        api->uart_puts("\n");

        void *file = api->open(argv[1]);
        if (!file) {
            api->uart_puts("[MP] Error: cannot open file\n");
            mp_deinit();
            return 1;
        }
        int size = api->file_size(file);
        char *buf = api->malloc(size + 1);
        api->read(file, buf, size, 0);

        // Strip \r
        int j = 0;
        for (int k = 0; k < size; k++) {
            if (buf[k] != '\r') buf[j++] = buf[k];
        }
        buf[j] = '\0';

        // Debug hex dump
        api->uart_puts("[MP] Size after strip: ");
        char hex[8];
        const char *hc = "0123456789ABCDEF";
        hex[0] = hc[(j >> 12) & 0xF];
        hex[1] = hc[(j >> 8) & 0xF];
        hex[2] = hc[(j >> 4) & 0xF];
        hex[3] = hc[j & 0xF];
        hex[4] = '\n';
        hex[5] = '\0';
        api->uart_puts(hex);

        api->uart_puts("[MP] Content:\n");
        api->uart_puts(buf);
        api->uart_puts("\n[MP] End content\n");

        // Execute using mp_parse_compile_execute with FILE_INPUT mode
        api->uart_puts("[MP] Creating lexer...\n");
        mp_lexer_t *lex = mp_lexer_new_from_str_len(qstr_from_str(argv[1]), buf, j, 0);
        api->uart_puts("[MP] Parsing...\n");

        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
            api->uart_puts("[MP] Compiling...\n");
            mp_obj_t module_fun = mp_compile(&parse_tree, lex->source_name, false);
            api->uart_puts("[MP] Executing...\n");
            mp_call_function_0(module_fun);
            nlr_pop();
            api->uart_puts("[MP] Done!\n");
        } else {
            // Exception - print it
            api->uart_puts("[MP] Exception caught\n");
            mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
            ret = 1;
        }
        api->free(buf);
    } else {
        // Interactive REPL
        // Use stdio hooks if available (for terminal emulator)
        if (api->stdio_puts) {
            api->stdio_puts("MicroPython for VibeOS\n");
        } else {
            api->puts("MicroPython for VibeOS\n");
        }
        pyexec_friendly_repl();
    }

    mp_deinit();
    return ret;
}

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

// Required stubs
mp_lexer_t *mp_lexer_new_from_file(qstr filename) {
    mp_raise_OSError(MP_ENOENT);
}

mp_import_stat_t mp_import_stat(const char *path) {
    return MP_IMPORT_STAT_NO_EXIST;
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

    // Print banner
    api->puts("MicroPython for VibeOS\n");
    api->puts(">>> ");

    // Run REPL
    pyexec_friendly_repl();

    mp_deinit();
    return 0;
}

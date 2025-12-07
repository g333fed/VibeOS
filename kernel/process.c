/*
 * VibeOS Process Management
 *
 * Win3.1 style - programs run in kernel space and call kernel functions directly.
 */

#include "process.h"
#include "elf.h"
#include "vfs.h"
#include "memory.h"
#include "string.h"
#include "printf.h"
#include "kapi.h"
#include <stddef.h>

// For now, just one process at a time (simple single-tasking)
static process_t current_proc;
static int next_pid = 1;

// Program entry point signature: int main(kapi_t *api, int argc, char **argv)
typedef int (*program_entry_t)(kapi_t *api, int argc, char **argv);

void process_init(void) {
    memset(&current_proc, 0, sizeof(current_proc));
    printf("[PROC] Process subsystem initialized\n");
}

process_t *process_current(void) {
    return current_proc.running ? &current_proc : NULL;
}

// Called when process wants to exit explicitly via kapi.exit()
void process_exit(int status) {
    printf("[PROC] Process '%s' (pid %d) exited with status %d\n",
           current_proc.name, current_proc.pid, status);

    current_proc.exit_status = status;
    current_proc.running = 0;

    // Free process stack
    if (current_proc.stack_base) {
        free(current_proc.stack_base);
        current_proc.stack_base = NULL;
    }

    // Note: With direct function calls, programs should just return from main().
    // This exit function is here for programs that want to exit early.
    // Since we can't easily longjmp back, calling exit() will hang.
    // TODO: Implement proper early exit if needed
    while (1) {}
}

int process_exec_args(const char *path, int argc, char **argv) {
    // Look up file in VFS
    vfs_node_t *file = vfs_lookup(path);
    if (!file) {
        printf("[PROC] exec: '%s' not found\n", path);
        return -1;
    }

    if (vfs_is_dir(file)) {
        printf("[PROC] exec: '%s' is a directory\n", path);
        return -2;
    }

    // Read the entire file
    size_t size = file->size;
    if (size == 0) {
        printf("[PROC] exec: '%s' is empty\n", path);
        return -3;
    }

    char *data = malloc(size);
    if (!data) {
        printf("[PROC] exec: out of memory\n");
        return -4;
    }

    int bytes = vfs_read(file, data, size, 0);
    if (bytes != (int)size) {
        printf("[PROC] exec: failed to read file\n");
        free(data);
        return -5;
    }

    // Validate and load ELF
    uint64_t entry = elf_load(data, size);
    free(data);

    if (entry == 0) {
        printf("[PROC] exec: failed to load ELF\n");
        return -6;
    }

    // Set up process
    current_proc.pid = next_pid++;
    strncpy(current_proc.name, path, PROCESS_NAME_MAX - 1);
    current_proc.name[PROCESS_NAME_MAX - 1] = '\0';
    current_proc.entry = entry;

    // Allocate stack
    current_proc.stack_base = malloc(PROCESS_STACK_SIZE);
    if (!current_proc.stack_base) {
        printf("[PROC] exec: failed to allocate stack\n");
        return -7;
    }

    // Stack grows down, so SP starts at top
    current_proc.sp = (uint64_t)current_proc.stack_base + PROCESS_STACK_SIZE;
    // Align to 16 bytes (required by AArch64 ABI)
    current_proc.sp &= ~0xFUL;

    current_proc.running = 1;

    printf("[PROC] Starting '%s' (pid %d) at 0x%lx\n",
           current_proc.name, current_proc.pid, entry);

    // Call program directly - it's just a function!
    // Entry point is: int main(kapi_t *api, int argc, char **argv)
    program_entry_t prog_main = (program_entry_t)entry;
    printf("[PROC] Calling program at %p with kapi at %p, argc=%d\n", (void*)prog_main, (void*)&kapi, argc);
    int result = prog_main(&kapi, argc, argv);
    printf("[PROC] Program returned!\n");

    // Program returned normally
    current_proc.running = 0;
    current_proc.exit_status = result;

    // Free stack
    if (current_proc.stack_base) {
        free(current_proc.stack_base);
        current_proc.stack_base = NULL;
    }

    printf("[PROC] Process '%s' returned %d\n", current_proc.name, result);
    return result;
}

// Wrapper for exec without arguments
int process_exec(const char *path) {
    char *argv[1] = { (char *)path };
    return process_exec_args(path, 1, argv);
}

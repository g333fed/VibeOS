/*
 * VibeOS Process Management
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>

#define PROCESS_NAME_MAX 32
#define PROCESS_STACK_SIZE 0x4000  // 16KB per process

typedef struct process {
    int pid;
    char name[PROCESS_NAME_MAX];
    uint64_t entry;           // Entry point
    uint64_t sp;              // Stack pointer
    void *stack_base;         // Stack allocation base
    int exit_status;
    int running;
} process_t;

// Initialize process subsystem
void process_init(void);

// Execute an ELF binary from VFS path
int process_exec(const char *path);
int process_exec_args(const char *path, int argc, char **argv);

// Exit current process
void process_exit(int status);

// Get current process
process_t *process_current(void);

#endif

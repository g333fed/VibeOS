/*
 * VibeOS Memory Management
 */

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

// Memory layout
#define RAM_START       0x40000000
#define RAM_SIZE        0x10000000  // 256MB
#define RAM_END         (RAM_START + RAM_SIZE)

// Heap starts after kernel (we'll set this at runtime)
extern uint64_t heap_start;
extern uint64_t heap_end;

// Initialize memory management
void memory_init(void);

// Simple heap allocator
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

// Memory stats
size_t memory_used(void);
size_t memory_free(void);

#endif

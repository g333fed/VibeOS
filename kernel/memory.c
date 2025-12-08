/*
 * VibeOS Memory Management
 *
 * Simple first-fit heap allocator. Not the fastest, but easy to understand.
 * Each allocation has a header with size and free flag.
 */

#include "memory.h"

uint64_t heap_start;
uint64_t heap_end;

// Block header - sits before each allocation
typedef struct block_header {
    size_t size;                    // Size of data area (not including header)
    uint8_t is_free;                // 1 if block is free, 0 if allocated
    struct block_header *next;      // Next block in list
} block_header_t;

#define HEADER_SIZE sizeof(block_header_t)
#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

static block_header_t *free_list = NULL;

// Defined in linker script - end of BSS in RAM
extern uint64_t _bss_end;

// Programs load at 0x41000000+, so heap must end before that
#define PROGRAM_LOAD_AREA 0x41000000

void memory_init(void) {
    // Heap starts after BSS (in RAM), aligned to 16 bytes
    // Add 64KB buffer for stack safety
    heap_start = ALIGN_UP((uint64_t)&_bss_end + 0x10000, 16);
    // Heap ends BEFORE the program load area to avoid overlap
    heap_end = PROGRAM_LOAD_AREA;

    // Initialize with one giant free block
    free_list = (block_header_t *)heap_start;
    free_list->size = heap_end - heap_start - HEADER_SIZE;
    free_list->is_free = 1;
    free_list->next = NULL;

    // Debug: print heap range
    extern void printf(const char *fmt, ...);
    printf("[MEM] Heap: 0x%lx - 0x%lx\n", heap_start, heap_end);
}

void *malloc(size_t size) {
    if (size == 0) return NULL;

    // Align size to 16 bytes
    size = ALIGN_UP(size, 16);

    block_header_t *current = free_list;

    // First-fit: find first block that's big enough
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            // Found a suitable block

            // Split if there's enough room for another block
            if (current->size >= size + HEADER_SIZE + 16) {
                // Create new block after this allocation
                block_header_t *new_block = (block_header_t *)((uint8_t *)current + HEADER_SIZE + size);
                new_block->size = current->size - size - HEADER_SIZE;
                new_block->is_free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            current->is_free = 0;
            return (void *)((uint8_t *)current + HEADER_SIZE);
        }
        current = current->next;
    }

    // No suitable block found
    return NULL;
}

void free(void *ptr) {
    if (ptr == NULL) return;

    // Get header
    block_header_t *block = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    block->is_free = 1;

    // Coalesce adjacent free blocks
    block_header_t *current = free_list;
    while (current != NULL) {
        if (current->is_free && current->next != NULL && current->next->is_free) {
            // Merge with next block
            current->size += HEADER_SIZE + current->next->size;
            current->next = current->next->next;
            // Don't advance - check if we can merge again
        } else {
            current = current->next;
        }
    }
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr != NULL) {
        // Zero the memory
        uint8_t *p = (uint8_t *)ptr;
        for (size_t i = 0; i < total; i++) {
            p[i] = 0;
        }
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    block_header_t *block = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);

    // If current block is big enough, just return it
    if (block->size >= size) {
        return ptr;
    }

    // Otherwise allocate new block and copy
    void *new_ptr = malloc(size);
    if (new_ptr != NULL) {
        uint8_t *src = (uint8_t *)ptr;
        uint8_t *dst = (uint8_t *)new_ptr;
        for (size_t i = 0; i < block->size; i++) {
            dst[i] = src[i];
        }
        free(ptr);
    }
    return new_ptr;
}

size_t memory_used(void) {
    size_t used = 0;
    block_header_t *current = free_list;
    while (current != NULL) {
        if (!current->is_free) {
            used += current->size + HEADER_SIZE;
        }
        current = current->next;
    }
    return used;
}

size_t memory_free(void) {
    size_t free_mem = 0;
    block_header_t *current = free_list;
    while (current != NULL) {
        if (current->is_free) {
            free_mem += current->size;
        }
        current = current->next;
    }
    return free_mem;
}

/*
 * VibeOS Kernel
 *
 * The main kernel entry point and core functionality.
 */

#include <stdint.h>
#include "memory.h"
#include "string.h"
#include "printf.h"
#include "fb.h"

// QEMU virt machine PL011 UART base address
#define UART0_BASE 0x09000000

// PL011 UART registers
#define UART_DR     (*(volatile uint32_t *)(UART0_BASE + 0x00))  // Data Register
#define UART_FR     (*(volatile uint32_t *)(UART0_BASE + 0x18))  // Flag Register
#define UART_FR_TXFF (1 << 5)  // Transmit FIFO Full

void uart_putc(char c) {
    // Wait until transmit FIFO is not full
    while (UART_FR & UART_FR_TXFF) {
        asm volatile("nop");
    }
    UART_DR = c;
}

void kernel_main(void) {
    // Raw UART test first
    uart_putc('V');
    uart_putc('I');
    uart_putc('B');
    uart_putc('E');
    uart_putc('\r');
    uart_putc('\n');

    // Test printf with simplest possible case
    uart_putc('1');
    printf("test");
    uart_putc('2');
    printf("\n");
    printf("  ╦  ╦╦╔╗ ╔═╗╔═╗╔═╗\n");
    printf("  ╚╗╔╝║╠╩╗║╣ ║ ║╚═╗\n");
    printf("   ╚╝ ╩╚═╝╚═╝╚═╝╚═╝\n");
    printf("\n");
    printf("VibeOS v0.1 - aarch64\n");
    printf("=====================\n\n");
    printf("[BOOT] Kernel loaded successfully!\n");
    printf("[BOOT] UART initialized.\n");

    // Initialize memory management
    memory_init();
    printf("[BOOT] Memory initialized.\n");
    printf("       Heap: %p - %p\n", (void *)heap_start, (void *)heap_end);
    printf("       Free: %lu MB\n", memory_free() / 1024 / 1024);

    // Test malloc
    printf("[TEST] Testing malloc...\n");
    char *test1 = malloc(100);
    char *test2 = malloc(200);
    printf("       Allocated 100 bytes at: %p\n", test1);
    printf("       Allocated 200 bytes at: %p\n", test2);

    // Write something to prove it works
    strcpy(test1, "Hi from printf!");
    printf("       Wrote to memory: %s\n", test1);

    // Free and check
    free(test1);
    free(test2);
    printf("       Freed allocations. Free: %lu MB\n", memory_free() / 1024 / 1024);

    printf("[BOOT] Running on QEMU virt machine.\n");

    // Initialize framebuffer
    if (fb_init() == 0) {
        // Clear to black
        fb_clear(COLOR_BLACK);

        // Draw "VibeOS" title
        fb_draw_string(20, 20, "VibeOS v0.1", COLOR_WHITE, COLOR_BLACK);
        fb_draw_string(20, 40, "================================", COLOR_WHITE, COLOR_BLACK);
        fb_draw_string(20, 60, "The vibes are immaculate.", COLOR_GREEN, COLOR_BLACK);
        fb_draw_string(20, 100, "Framebuffer: 800x600", COLOR_WHITE, COLOR_BLACK);
        fb_draw_string(20, 120, "Font: 8x16 bitmap", COLOR_WHITE, COLOR_BLACK);
        fb_draw_string(20, 160, "Ready for more awesome stuff!", COLOR_AMBER, COLOR_BLACK);

        printf("[FB] Text drawn on screen!\n");
    }

    printf("\n");
    printf("Welcome to VibeOS! The vibes are immaculate.\n");
    printf("\n");

    // For now, just halt
    printf("[KERNEL] Entering idle loop...\n");

    while (1) {
        asm volatile("wfe");
    }
}

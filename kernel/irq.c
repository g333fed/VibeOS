/*
 * VibeOS Interrupt Handling - Shared Code
 *
 * Platform-specific drivers are in hal/qemu/irq.c and hal/pizero2w/irq.c.
 * This file contains:
 * - Exception handlers (sync, FIQ, SError) shared by all platforms
 * - Legacy API wrappers for QEMU compatibility
 */

#include "irq.h"
#include "printf.h"
#include "hal/hal.h"

// ============================================================================
// Legacy API Wrappers (for QEMU code compatibility)
// These call through to HAL functions
// ============================================================================

void irq_init(void) {
    hal_irq_init();
}

void irq_enable(void) {
    hal_irq_enable();
}

void irq_disable(void) {
    hal_irq_disable();
}

void irq_enable_irq(uint32_t irq) {
    hal_irq_enable_irq(irq);
}

void irq_disable_irq(uint32_t irq) {
    hal_irq_disable_irq(irq);
}

void irq_register_handler(uint32_t irq, irq_handler_t handler) {
    hal_irq_register_handler(irq, handler);
}

void timer_init(uint32_t interval_ms) {
    hal_timer_init(interval_ms);
}

uint64_t timer_get_ticks(void) {
    return hal_timer_get_ticks();
}

void timer_set_interval(uint32_t interval_ms) {
    hal_timer_set_interval(interval_ms);
}

void wfi(void) {
    asm volatile("wfi");
}

void sleep_ms(uint32_t ms) {
    // Timer runs at 100Hz (10ms per tick)
    uint64_t ticks_to_wait = (ms + 9) / 10;
    if (ticks_to_wait == 0) ticks_to_wait = 1;

    uint64_t target = hal_timer_get_ticks() + ticks_to_wait;
    while (hal_timer_get_ticks() < target) {
        wfi();
    }
}

// ============================================================================
// Shared Exception Handlers (used by all platforms)
// Called from vectors.S
// ============================================================================

void handle_sync_exception(uint64_t esr, uint64_t elr, uint64_t far) {
    uint32_t ec = (esr >> 26) & 0x3F;
    uint32_t iss = esr & 0x1FFFFFF;

    printf("\n");
    printf("==========================================\n");
    printf("  KERNEL PANIC: Synchronous Exception\n");
    printf("==========================================\n");
    printf("  ESR_EL1: 0x%08lx\n", esr);
    printf("  ELR_EL1: 0x%016lx (return address)\n", elr);
    printf("  FAR_EL1: 0x%016lx (fault address)\n", far);
    printf("\n");
    printf("  Exception Class (EC): 0x%02x = ", ec);

    switch (ec) {
        case 0x00: printf("Unknown reason\n"); break;
        case 0x01: printf("Trapped WFI/WFE\n"); break;
        case 0x0E: printf("Illegal execution state\n"); break;
        case 0x15: printf("SVC instruction (syscall)\n"); break;
        case 0x20: printf("Instruction abort from lower EL\n"); break;
        case 0x21: printf("Instruction abort from current EL\n"); break;
        case 0x22: printf("PC alignment fault\n"); break;
        case 0x24: printf("Data abort from lower EL\n"); break;
        case 0x25: printf("Data abort from current EL\n"); break;
        case 0x26: printf("SP alignment fault\n"); break;
        case 0x2C: printf("Floating-point exception\n"); break;
        default:   printf("(see ARM ARM)\n"); break;
    }

    printf("  ISS: 0x%06x\n", iss);

    if (ec == 0x24 || ec == 0x25 || ec == 0x20 || ec == 0x21) {
        printf("  Access type: %s\n", (iss & (1 << 6)) ? "Write" : "Read");
        printf("  DFSC/IFSC: 0x%02x\n", iss & 0x3F);
    }

    printf("\n");
    printf("  System halted.\n");
    printf("==========================================\n");

    hal_irq_disable();
    while (1) {
        asm volatile("wfi");
    }
}

void handle_fiq(void) {
    printf("[IRQ] FIQ received (unexpected)\n");
}

void handle_serror(uint64_t esr) {
    printf("\n");
    printf("==========================================\n");
    printf("  KERNEL PANIC: SError (Async Abort)\n");
    printf("==========================================\n");
    printf("  ESR_EL1: 0x%08lx\n", esr);
    printf("  System halted.\n");
    printf("==========================================\n");

    hal_irq_disable();
    while (1) {
        asm volatile("wfi");
    }
}

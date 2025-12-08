/*
 * VibeOS Exception Vector Table
 *
 * AArch64 exception vectors must be 2KB aligned, with each entry 128 bytes.
 * There are 16 entries total (4 exception types x 4 exception sources).
 */

.section .text

// Each vector entry is 128 bytes (32 instructions max)
.macro VECTOR_ENTRY label
.align 7
\label:
.endm

// Save all general-purpose registers
.macro SAVE_REGS
    sub     sp, sp, #256
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    str     x30, [sp, #240]
.endm

// Restore all general-purpose registers
.macro RESTORE_REGS
    ldp     x0, x1, [sp, #0]
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldp     x6, x7, [sp, #48]
    ldp     x8, x9, [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    ldr     x30, [sp, #240]
    add     sp, sp, #256
.endm

/*
 * Exception Vector Table
 * Must be 2KB (0x800) aligned
 *
 * Vector offsets:
 *   0x000 - Synchronous, Current EL with SP0
 *   0x080 - IRQ, Current EL with SP0
 *   0x100 - FIQ, Current EL with SP0
 *   0x180 - SError, Current EL with SP0
 *   0x200 - Synchronous, Current EL with SPx
 *   0x280 - IRQ, Current EL with SPx
 *   0x300 - FIQ, Current EL with SPx
 *   0x380 - SError, Current EL with SPx
 *   0x400 - Synchronous, Lower EL using AArch64
 *   0x480 - IRQ, Lower EL using AArch64
 *   0x500 - FIQ, Lower EL using AArch64
 *   0x580 - SError, Lower EL using AArch64
 *   0x600 - Synchronous, Lower EL using AArch32
 *   0x680 - IRQ, Lower EL using AArch32
 *   0x700 - FIQ, Lower EL using AArch32
 *   0x780 - SError, Lower EL using AArch32
 */

.align 11   // 2KB aligned
.global exception_vectors
exception_vectors:

// Current EL with SP0 (not used - we use SPx)
VECTOR_ENTRY vec_sync_sp0
    b       sync_handler

VECTOR_ENTRY vec_irq_sp0
    b       irq_handler_entry

VECTOR_ENTRY vec_fiq_sp0
    b       fiq_handler

VECTOR_ENTRY vec_serror_sp0
    b       serror_handler

// Current EL with SPx (this is what we use)
VECTOR_ENTRY vec_sync_spx
    b       sync_handler

VECTOR_ENTRY vec_irq_spx
    b       irq_handler_entry

VECTOR_ENTRY vec_fiq_spx
    b       fiq_handler

VECTOR_ENTRY vec_serror_spx
    b       serror_handler

// Lower EL using AArch64 (userspace)
VECTOR_ENTRY vec_sync_a64
    b       sync_handler

VECTOR_ENTRY vec_irq_a64
    b       irq_handler_entry

VECTOR_ENTRY vec_fiq_a64
    b       fiq_handler

VECTOR_ENTRY vec_serror_a64
    b       serror_handler

// Lower EL using AArch32 (not supported)
VECTOR_ENTRY vec_sync_a32
    b       sync_handler

VECTOR_ENTRY vec_irq_a32
    b       irq_handler_entry

VECTOR_ENTRY vec_fiq_a32
    b       fiq_handler

VECTOR_ENTRY vec_serror_a32
    b       serror_handler

/*
 * Exception Handlers
 */

// Synchronous exception (data abort, instruction abort, SVC, etc.)
sync_handler:
    SAVE_REGS

    // Get exception info
    mrs     x0, esr_el1     // Exception Syndrome Register
    mrs     x1, elr_el1     // Exception Link Register (return address)
    mrs     x2, far_el1     // Fault Address Register

    // Call C handler
    bl      handle_sync_exception

    RESTORE_REGS
    eret

// IRQ handler
irq_handler_entry:
    SAVE_REGS

    // Call C handler
    bl      handle_irq

    RESTORE_REGS
    eret

// FIQ handler (not used)
fiq_handler:
    SAVE_REGS
    bl      handle_fiq
    RESTORE_REGS
    eret

// SError handler (asynchronous abort)
serror_handler:
    SAVE_REGS
    mrs     x0, esr_el1
    bl      handle_serror
    RESTORE_REGS
    eret

/*
 * VibeOS - aarch64 Bootloader
 *
 * Entry point for the kernel. Sets up the stack and jumps to C code.
 * Targets QEMU virt machine.
 */

.section ".text.boot"

.global _start

_start:
    // Get the CPU ID - only CPU 0 should initialize
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    cbz     x0, primary_cpu

    // Secondary CPUs go to sleep
secondary_cpu:
    wfe
    b       secondary_cpu

primary_cpu:
    // Debug: print current EL to UART
    mov     x1, #0x09000000
    mov     w2, #'E'
    str     w2, [x1]
    mov     w2, #'L'
    str     w2, [x1]

    // Check current exception level
    mrs     x0, CurrentEL
    lsr     x0, x0, #2          // CurrentEL is in bits [3:2]

    // Print EL number
    add     w2, w0, #'0'
    str     w2, [x1]
    mov     w2, #'\n'
    str     w2, [x1]

    cmp     x0, #2
    b.eq    drop_to_el1
    cmp     x0, #1
    b.eq    at_el1
    // If EL3, we'd need more setup - just hang for now
    b       secondary_cpu

drop_to_el1:
    // We're at EL2, need to drop to EL1

    // Enable AArch64 for EL1
    mov     x0, #(1 << 31)      // RW=1: EL1 is AArch64
    msr     hcr_el2, x0

    // Set up SPSR for EL1h (EL1 with SP_EL1)
    mov     x0, #0x3c5          // DAIF masked, EL1h
    msr     spsr_el2, x0

    // Set return address to at_el1
    adr     x0, at_el1
    msr     elr_el2, x0

    // Return to EL1
    eret

at_el1:
    // x9 = UART base for debug output
    mov     x9, #0x09000000

    // Debug: confirm we're at EL1
    mov     w10, #'!'
    str     w10, [x9]
    mov     w10, #'1'
    str     w10, [x9]
    mov     w10, #'\n'
    str     w10, [x9]

    // Set up the stack pointer (stack grows downward)
    mov     w10, #'S'
    str     w10, [x9]
    ldr     x0, =_stack_top
    mov     sp, x0
    mov     w10, #'s'
    str     w10, [x9]

    // Clear BSS section
    mov     w10, #'B'
    str     w10, [x9]
    ldr     x0, =_bss_start
    ldr     x1, =_bss_end
clear_bss:
    cmp     x0, x1
    b.ge    bss_done
    str     xzr, [x0], #8
    b       clear_bss
bss_done:
    mov     w10, #'b'
    str     w10, [x9]

    // Jump to kernel main
    mov     w10, #'K'
    str     w10, [x9]
    bl      kernel_main

    // If kernel_main returns, halt
halt:
    wfe
    b       halt

// Stack is in its own section, placed after BSS by linker
.section ".stack", "aw", @nobits
.align 16
_stack_bottom:
    .skip 0x10000  // 64KB stack
_stack_top:

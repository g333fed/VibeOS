/*
 * VibeOS C Runtime Startup
 *
 * Entry point for all userspace programs.
 * Receives: x0=kapi_t*, x1=argc, x2=argv
 */

.section ".text.start"
.global _start

_start:
    // x0 = kapi_t*, x1 = argc, x2 = argv
    // x30 contains return address to kernel - save it!

    // Save x30 (return address) and x29 (frame pointer)
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    // Call main(kapi_t *api, int argc, char **argv)
    // x0, x1, x2 already set by kernel
    bl      main

    // Restore x29 and x30, then return to kernel
    ldp     x29, x30, [sp], #16
    ret

/*
 * VibeOS Userspace Linker Script
 */

ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);  /* R-X */
}

SECTIONS
{
    . = 0x40200000;

    .text : {
        *(.text.start)
        *(.text*)
        *(.rodata*)
        *(.data*)
    } :text

    .bss : {
        *(.bss*)
        *(COMMON)
    } :text

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.ARM.*)
    }
}

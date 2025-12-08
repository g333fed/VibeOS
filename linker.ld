/*
 * VibeOS Linker Script
 *
 * Memory layout for QEMU virt machine (aarch64)
 * Using -bios mode: flash starts at 0x0, RAM at 0x40000000
 * We boot at EL3 with full hardware access
 *
 * Code runs from flash (0x0), data/BSS in RAM (0x40000000+)
 */

ENTRY(_start)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 64M
    RAM   (rwx) : ORIGIN = 0x40000000, LENGTH = 256M
}

SECTIONS
{
    /* Boot code at start of flash */
    .text.boot : {
        *(.text.boot)
    } > FLASH

    /* Rest of code in flash */
    .text : {
        *(.text)
        *(.text.*)
    } > FLASH

    /* Read-only data in flash */
    .rodata : ALIGN(8) {
        *(.rodata)
        *(.rodata.*)
    } > FLASH

    /* End of flash content - save for data copy */
    _data_load = .;

    /* Initialized data - loaded from flash, runs in RAM */
    .data : AT(_data_load) {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    } > RAM

    /* BSS section (uninitialized data) in RAM */
    .bss : {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    } > RAM

    /* Discard unneeded sections */
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.stack)
    }
}

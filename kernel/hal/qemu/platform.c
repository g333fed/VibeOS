/*
 * QEMU virt machine Platform Info
 */

#include "../hal.h"

const char *hal_platform_name(void) {
    return "QEMU virt (aarch64)";
}

void hal_wfi(void) {
    asm volatile("wfi");
}

// QEMU uses virtio for input, not USB
int hal_usb_init(void) {
    return -1;  // Not supported on QEMU virt
}

int hal_usb_keyboard_poll(uint8_t *report, int report_len) {
    (void)report;
    (void)report_len;
    return -1;
}

void hal_usb_keyboard_tick(void) {
    // Not supported on QEMU
}

// QEMU has no physical LED
void hal_led_init(void) {
    // No LED on QEMU virt
}

void hal_led_on(void) {
    // No LED on QEMU virt
}

void hal_led_off(void) {
    // No LED on QEMU virt
}

void hal_led_toggle(void) {
    // No LED on QEMU virt
}

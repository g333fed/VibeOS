/*
 * Raspberry Pi Zero 2W Platform Info
 */

#include "../hal.h"
#include "usb/usb_types.h"

// Pi system timer (1MHz free-running counter)
#define PI_SYSTIMER_LO  (*(volatile uint32_t *)0x3F003004)

const char *hal_platform_name(void) {
    return "Raspberry Pi Zero 2W";
}

void hal_wfi(void) {
    asm volatile("wfi");
}

// Microsecond timer - reads directly from Pi system timer
// Available very early in boot, no initialization required
uint32_t hal_get_time_us(void) {
    return PI_SYSTIMER_LO;
}

// CPU Info - BCM2710 with Cortex-A53 cores
const char *hal_get_cpu_name(void) {
    return "Cortex-A53";
}

uint32_t hal_get_cpu_freq_mhz(void) {
    return 1000;  // Pi Zero 2W runs at 1GHz
}

int hal_get_cpu_cores(void) {
    return 4;  // Pi Zero 2W has 4 cores
}

// USB Device List
int hal_usb_get_device_count(void) {
    return usb_state.num_devices;
}

int hal_usb_get_device_info(int idx, uint16_t *vid, uint16_t *pid,
                            char *name, int name_len) {
    if (idx < 0 || idx >= usb_state.num_devices) {
        return -1;
    }

    usb_device_t *dev = &usb_state.devices[idx];

    // VID/PID not stored in current implementation - return 0
    if (vid) *vid = 0;
    if (pid) *pid = 0;

    // Generate name from device type
    if (name && name_len > 0) {
        const char *desc;
        if (dev->is_hub) {
            desc = "USB Hub";
        } else if (usb_state.keyboard_addr == dev->address) {
            desc = "USB Keyboard";
        } else {
            desc = "USB Device";
        }

        // Copy name
        int i;
        for (i = 0; desc[i] && i < name_len - 1; i++) {
            name[i] = desc[i];
        }
        name[i] = '\0';
    }

    return 0;
}

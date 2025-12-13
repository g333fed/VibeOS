/*
 * blink - Blink the ACT LED on Raspberry Pi
 *
 * Blinks the green activity LED 3 times.
 * Only works on Pi hardware (no-op on QEMU).
 */

#include "../lib/vibe.h"

// Simple spin delay using ARM system counter
static void delay_ms(uint32_t ms) {
    uint64_t start, now, freq;

    // Read counter frequency (usually 54MHz on Pi)
    asm volatile("mrs %0, cntfrq_el0" : "=r" (freq));

    // Read current counter
    asm volatile("mrs %0, cntpct_el0" : "=r" (start));

    // Calculate ticks to wait
    uint64_t ticks = (freq * ms) / 1000;

    // Spin until elapsed
    do {
        asm volatile("mrs %0, cntpct_el0" : "=r" (now));
    } while ((now - start) < ticks);
}

int main(kapi_t *k, int argc, char **argv) {
    (void)argc; (void)argv;

    k->puts("Blinking LED 3 times...\n");

    for (int i = 0; i < 3; i++) {
        k->led_on();
        delay_ms(500);
        k->led_off();
        delay_ms(500);
    }

    k->puts("Done!\n");
    return 0;
}

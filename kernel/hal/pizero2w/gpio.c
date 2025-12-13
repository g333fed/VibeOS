/*
 * Raspberry Pi Zero 2W GPIO Driver
 *
 * Provides control for the ACT LED (GPIO 47) for visual debugging.
 */

#include "../hal.h"
#include "../../printf.h"

// ============================================================================
// GPIO Register Definitions
// ============================================================================

#define GPIO_BASE           0x3F200000

// Function select registers (3 bits per GPIO, 10 GPIOs per register)
#define GPFSEL0             (*(volatile uint32_t *)(GPIO_BASE + 0x00))  // GPIO 0-9
#define GPFSEL1             (*(volatile uint32_t *)(GPIO_BASE + 0x04))  // GPIO 10-19
#define GPFSEL2             (*(volatile uint32_t *)(GPIO_BASE + 0x08))  // GPIO 20-29
#define GPFSEL3             (*(volatile uint32_t *)(GPIO_BASE + 0x0C))  // GPIO 30-39
#define GPFSEL4             (*(volatile uint32_t *)(GPIO_BASE + 0x10))  // GPIO 40-49
#define GPFSEL5             (*(volatile uint32_t *)(GPIO_BASE + 0x14))  // GPIO 50-53

// Output set registers (1 bit per GPIO)
#define GPSET0              (*(volatile uint32_t *)(GPIO_BASE + 0x1C))  // GPIO 0-31
#define GPSET1              (*(volatile uint32_t *)(GPIO_BASE + 0x20))  // GPIO 32-53

// Output clear registers (1 bit per GPIO)
#define GPCLR0              (*(volatile uint32_t *)(GPIO_BASE + 0x28))  // GPIO 0-31
#define GPCLR1              (*(volatile uint32_t *)(GPIO_BASE + 0x2C))  // GPIO 32-53

// Pin level registers (read actual pin state)
#define GPLEV0              (*(volatile uint32_t *)(GPIO_BASE + 0x34))  // GPIO 0-31
#define GPLEV1              (*(volatile uint32_t *)(GPIO_BASE + 0x38))  // GPIO 32-53

// Function select values
#define GPIO_FUNC_INPUT     0
#define GPIO_FUNC_OUTPUT    1
#define GPIO_FUNC_ALT0      4
#define GPIO_FUNC_ALT1      5
#define GPIO_FUNC_ALT2      6
#define GPIO_FUNC_ALT3      7
#define GPIO_FUNC_ALT4      3
#define GPIO_FUNC_ALT5      2

// ACT LED is on GPIO 29 for Pi Zero 2W (same as Pi 3B+)
// Note: Pi Zero/Zero W use GPIO 47, but Zero 2W is different!
#define ACT_LED_GPIO        29
#define ACT_LED_BIT         (1 << ACT_LED_GPIO)  // Bit 29 in bank 0

// ============================================================================
// LED State
// ============================================================================

static int led_state = 0;

// Forward declarations
void led_on(void);
void led_off(void);

// ============================================================================
// Memory Barrier
// ============================================================================

static inline void dsb(void) {
    asm volatile("dsb sy" ::: "memory");
}

// ============================================================================
// LED Functions
// ============================================================================

void led_init(void) {
    // GPIO 29 is in GPFSEL2 (GPIOs 20-29)
    // GPIO 29 = (29 - 20) = 9th GPIO in GPFSEL2
    // Bits [29:27] control GPIO 29 function
    //
    // Function select: 3 bits per GPIO
    // GPIO 20: bits [2:0]
    // GPIO 21: bits [5:3]
    // GPIO 22: bits [8:6]
    // GPIO 23: bits [11:9]
    // GPIO 24: bits [14:12]
    // GPIO 25: bits [17:15]
    // GPIO 26: bits [20:18]
    // GPIO 27: bits [23:21]
    // GPIO 28: bits [26:24]
    // GPIO 29: bits [29:27]

    uint32_t sel = GPFSEL2;

    // Clear bits [29:27]
    sel &= ~(7 << 27);

    // Set to output (001)
    sel |= (GPIO_FUNC_OUTPUT << 27);

    GPFSEL2 = sel;
    dsb();

    // Start with LED off
    led_off();

    printf("[GPIO] ACT LED (GPIO 29) initialized\n");
}

void led_on(void) {
    // Zero 2W: Clear pin to turn LED ON (active low)
    GPCLR0 = ACT_LED_BIT;
    dsb();
    led_state = 1;
}

void led_off(void) {
    // Zero 2W: Set pin to turn LED OFF (active low)
    GPSET0 = ACT_LED_BIT;
    dsb();
    led_state = 0;
}

void led_toggle(void) {
    if (led_state) {
        led_off();
    } else {
        led_on();
    }
}

int led_get_state(void) {
    return led_state;
}

// ============================================================================
// HAL LED Interface
// ============================================================================

void hal_led_init(void) {
    led_init();
}

void hal_led_on(void) {
    led_on();
}

void hal_led_off(void) {
    led_off();
}

void hal_led_toggle(void) {
    led_toggle();
}

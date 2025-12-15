// MicroPython HAL for VibeOS

#ifndef MICROPY_INCLUDED_VIBEOS_MPHALPORT_H
#define MICROPY_INCLUDED_VIBEOS_MPHALPORT_H

#include "py/mpconfig.h"

// Console I/O
int mp_hal_stdin_rx_chr(void);
mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len);

// Timing
mp_uint_t mp_hal_ticks_ms(void);
mp_uint_t mp_hal_ticks_us(void);
mp_uint_t mp_hal_ticks_cpu(void);
void mp_hal_delay_ms(mp_uint_t ms);
void mp_hal_delay_us(mp_uint_t us);

// Interrupt character (Ctrl+C)
void mp_hal_set_interrupt_char(int c);

#endif

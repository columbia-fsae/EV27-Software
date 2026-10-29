/**
 * @file timebase.h
 * @brief Microsecond timestamps and delays from the Cortex-M4 DWT cycle counter
 */

#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>

void timebase_init(void);

// Microseconds since timebase_init(). Main-context only; must be called at least once every
// 25 s (one CYCCNT wrap at 170 MHz) so the wrap is tracked.
uint64_t timebase_now_us(void);

void timebase_delay_us(uint32_t us);

#endif /* TIMEBASE_H */

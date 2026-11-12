/**
 * @file timebase.c
 * @brief Microsecond timestamps and delays from the Cortex-M4 DWT cycle counter
 */

#include "timebase.h"

#include "main.h"

static uint32_t cycles_per_us;
static uint32_t last_cyccnt;
static uint64_t cycles_high;  // accumulated cycles at each CYCCNT wrap

void timebase_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    cycles_per_us = SystemCoreClock / 1000000U;
    last_cyccnt = 0;
    cycles_high = 0;
}

uint64_t timebase_now_us(void) {
    uint32_t now = DWT->CYCCNT;
    if (now < last_cyccnt) {
        cycles_high += (uint64_t)1 << 32;
    }
    last_cyccnt = now;
    return (cycles_high + now) / cycles_per_us;
}

void timebase_delay_us(uint32_t us) {
    uint32_t start = DWT->CYCCNT;
    uint32_t wait = us * cycles_per_us;
    while ((DWT->CYCCNT - start) < wait) {
    }
}

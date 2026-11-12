/**
 * @file isr_sim.h
 * @brief Runs an "interrupt handler" over and over on a background thread, so
 * a test can check whether data shared between an ISR and the main loop (or
 * an RTOS task) can be read half-updated.
 *
 * Model:
 *   - The simulated ISR runs as fast as it can on its own thread, the way a
 *     circular-DMA ADC callback fires continuously on the target.
 *   - The test thread plays the main loop / RTOS task.
 *   - Masking interrupts (__disable_irq, __set_PRIMASK(1), or isr_sim_mask()
 *     for anything else that acts as a critical section, such as
 *     taskENTER_CRITICAL) stops the simulated ISR from running until the
 *     matching unmask, the same guarantee you get on a single-core Cortex-M.
 *
 * Code with no protection gets no protection here either, so a torn read
 * shows up as soon as the two threads overlap.
 *
 * Limits: this is a stress test, not a proof. A pass means no tear was seen
 * in the iterations run. On the target the ISR also cannot be preempted by the
 * main loop, while here both threads run in parallel on different cores, which
 * only makes overlaps more likely.
 */
#ifndef ISR_SIM_H
#define ISR_SIM_H

#include <stdint.h>

typedef void (*isr_sim_handler_t)(void);

// Point the __disable_irq/__enable_irq/__get_PRIMASK/__set_PRIMASK fakes at
// the simulator. Call after hal_fakes_reset(), which clears custom_fake.
void isr_sim_install_irq_fakes(void);

// Start calling handler in a loop on a background thread.
void isr_sim_start(isr_sim_handler_t handler);

// Stop the background thread. Returns how many times handler ran.
uint32_t isr_sim_stop(void);

// Block / unblock the simulated ISR from the calling thread. Nesting is not
// counted, matching PRIMASK: one unmask undoes any number of masks.
void isr_sim_mask(void);
void isr_sim_unmask(void);

#endif

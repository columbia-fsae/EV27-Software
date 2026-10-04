/**
 * @file bsm_config.h
 * @brief Tunable values for the battery state machine (BSM)
 *
 * Everything the BSM uses that you might reasonably want to retune (timeouts,
 * precharge voltage window) lives here instead of being scattered through
 * bsm.c.
 */

#ifndef BSM_CONFIG_H
#define BSM_CONFIG_H

// ============================================================================
// STATE TIMEOUTS AND DELAYS (ms)
// ============================================================================

// Precharge must finish (voltage window met) before this or the BSM faults
#define PRECHARGE_FAULT_TIME_MS 60000
// Precharge may not end before this, even if the voltage window is met
#define PRECHARGE_MIN_TIME_MS 25000
// Delay between closing IR+ and entering the driving state
#define PRECHARGE_POST_DELAY_MS 1000
// Time allowed for an isolation relay aux contact to confirm after commanding it closed
#define IR_FAULT_TIME_MS 1000

// ============================================================================
// PRECHARGE VOLTAGE WINDOW
// ============================================================================

// Minimum battery-side voltage (V) before precharge can complete
#define PRECHARGE_BAT_MIN_V 20.0f
// Tractive-system voltage must be within this fraction of battery voltage
// (lower and upper bound) for precharge to complete
#define PRECHARGE_TS_MIN_RATIO 0.905
#define PRECHARGE_TS_MAX_RATIO 1.05

#endif  // BSM_CONFIG_H

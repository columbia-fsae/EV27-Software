/**
 * @file chargersm_config.h
 * @brief Tunable values and status text for the charger state machine
 *
 * Sections:
 *   1. Test switches
 *   2. Timing
 *   3. Balancing and cell limits
 *   4. Fault decoding
 *   5. Status messages shown on the display
 */

#ifndef CHARGERSM_CONFIG_H
#define CHARGERSM_CONFIG_H

// ============================================================================
// 1. TEST SWITCHES
// ============================================================================

// Uncomment to disable SDC, BSM, and Elcon fault detection for testing without CAN
// #define TRAINING_WHEELS_MODE

// ============================================================================
// 2. TIMING (ms)
// ============================================================================

#define CHARGE_WAIT 600000  // Longest continuous charge before pausing
#define WAIT 10000          // Length of a pause between charge periods
#define BSM_TIMEOUT 5000    // No BSM CAN message for this long is a fault
#define ELCON_TIMEOUT 5000  // No Elcon CAN message for this long is a fault

// ============================================================================
// 3. BALANCING AND CELL LIMITS
// ============================================================================

// Automatic mode balances once pack voltage passes this fraction of the voltage limit...
#define BALANCING_THRESH 0.9f
// ...and only re-arms balancing after it falls below this fraction
#define BALANCING_THRESH_MIN 0.85f

#define CELL_V_LIMIT 4200  // in mv

// ============================================================================
// 4. FAULT DECODING
// ============================================================================

// Number of defined fault bits in error_flags
#define NUM_FAULTS 11

// error_flags bits 0-4 carry the Elcon status faults (see datasheet)
#define ELCON_FAULT_MASK 0x1F
// Bit positions of the remaining faults
#define FAULT_BIT_ELCON_TIMEOUT 5
#define FAULT_BIT_TBP_NOT_READY 6
#define FAULT_BIT_BSM_FAULT 7
#define FAULT_BIT_BSM_TIMEOUT 8
#define FAULT_BIT_SDC 9
#define FAULT_BIT_CELL_OVERVOLTAGE 10

// BSM states reported by the tractive battery (see bsm.h in Tractive_Battery)
#define BSM_STATE_DRIVING 5  // pack is ready
#define BSM_STATE_FAULT 0xF

// ============================================================================
// 5. STATUS MESSAGES
// ============================================================================

// Status-line text is limited to the glyphs in the font (A-Z, ^ [ \ ], 0-9, space)

// Messages offered to the user while editing status; one per UIMessagePaths entry
#define STATUS_CHANGE_MSG_STAY "^ CANCEL"
#define STATUS_CHANGE_MSG_GO_TO_READY "^ STOP"
#define STATUS_CHANGE_MSG_MANUAL "^ MANUAL MODE"
#define STATUS_CHANGE_MSG_AUTOMATIC "^ AUTOMATIC MODE"
#define STATUS_CHANGE_MSG_CHARGE_MANUAL "^ CHARGE [MANUAL]"
#define STATUS_CHANGE_MSG_BALANCE_MANUAL "^ BALANCE [MANUAL]"
#define STATUS_CHANGE_MSG_WAIT_MANUAL "^ WAIT [MANUAL]"
#define STATUS_CHANGE_MSG_NO_OPTIONS "^ NO OPTIONS"

// Current state of the charger
#define STATUS_MSG_START "STARTING"
#define STATUS_MSG_READY "READY TO CHARGE"
#define STATUS_MSG_MANUAL_BALANCE "MANUAL [BALANCING]"
#define STATUS_MSG_MANUAL_CHARGE "MANUAL [CHARGING]"
#define STATUS_MSG_MANUAL_WAIT "MANUAL [WAITING]"
#define STATUS_MSG_AUTO_BALANCE "AUTOMATIC [BALANCING]"
#define STATUS_MSG_AUTO_CHARGE "AUTOMATIC [CHARGING]"
#define STATUS_MSG_AUTO_WAIT "AUTOMATIC [WAITING]"
#define STATUS_MSG_STM32_ERROR "STM32 ERROR"
#define STATUS_MSG_CAN_HAL_ERROR "STM32 HAL CAN ERR"

#endif  // CHARGERSM_CONFIG_H

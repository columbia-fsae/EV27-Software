/**
 * @file core_config.h
 * @brief Application-level configuration for the charging cart main loop
 *
 * Sections:
 *   1. Main loop
 *   2. Reset / fault recovery
 *   3. CAN receive filter
 *
 * Kept outside the CubeMX-generated blocks so it survives regenerating from the .ioc.
 * Included from the USER CODE section of main.h.
 */

#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

// ============================================================================
// 1. MAIN LOOP (ms)
// ============================================================================

// How often the CAN messages are sent and the display info is refreshed
#define CAN_UPDATE_PERIOD_MS 1000
// How long a CAN send error stays on the display
#define CAN_ERROR_DISPLAY_MS 1000

// ============================================================================
// 2. RESET / FAULT RECOVERY
// ============================================================================

// Memory access reset counter upon reset of the system
#define RESET_COUNTER_MAGIC 0xCAFEBABE

// Number of consecutive soft resets allowed by Error_Handler; after that it halts
#define MAX_SOFT_RESETS 10

// Pause before resetting so the error can be seen and sent (ms)
#define ERROR_HANDLER_DELAY_MS 100

// ============================================================================
// 3. CAN RECEIVE FILTER
// ============================================================================

// One mask filter; ID and mask of 0 accept every frame into FIFO0
#define CAN_FILTER_BANK 0
#define CAN_FILTER_ID_HIGH 0x0000
#define CAN_FILTER_ID_LOW 0x0000
#define CAN_FILTER_MASK_ID_HIGH 0x0000
#define CAN_FILTER_MASK_ID_LOW 0x0000
#define CAN_SLAVE_START_FILTER_BANK 20

#endif  // CORE_CONFIG_H

/**
 * @file core_config.h
 * @brief Application-level configuration for the main loop
 *
 * Sections:
 *   1. Main-loop task schedule
 *   2. Reset / fault recovery
 *   3. FDCAN receive filter
 *   4. Debug console
 *
 * Kept outside the CubeMX-generated blocks so it survives regenerating from the .ioc.
 * Included from the USER CODE section of main.h.
 */

#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

// ============================================================================
// 1. MAIN-LOOP TASK SCHEDULE (ms)
// ============================================================================

// Each task runs every PERIOD; OFFSET staggers tasks so they do not all fire on the same tick.
#define BMS_RUN_PERIOD_MS 1000
#define BMS_RUN_OFFSET_MS 0

#define BSM_RUN_PERIOD_MS 50
#define BSM_RUN_OFFSET_MS 0

#define BMS_CAN_STATS_PERIOD_MS 1000
#define BMS_CAN_STATS_OFFSET_MS 0

#define BMS_CAN_FAULTS_PERIOD_MS 10
#define BMS_CAN_FAULTS_OFFSET_MS 3

#define BMS_CAN_DATA_PERIOD_MS 40
#define BMS_CAN_DATA_OFFSET_MS 0

#define BMS_CAN_IDS_PERIOD_MS 2000
#define BMS_CAN_IDS_OFFSET_MS 300

#define SOC_CAN_STATS_PERIOD_MS 1000
#define SOC_CAN_STATS_OFFSET_MS 200

#define SOC_CAN_DATA_PERIOD_MS 40
#define SOC_CAN_DATA_OFFSET_MS 20

#define BSM_CAN_PERIOD_MS 50
#define BSM_CAN_OFFSET_MS 20

#define ADC_CAN_PERIOD_MS 1000
#define ADC_CAN_OFFSET_MS 350

#define ERROR_CAN_PERIOD_MS 23
#define ERROR_CAN_OFFSET_MS 0

// ============================================================================
// 2. RESET / FAULT RECOVERY
// ============================================================================

// Memory access reset counter upon reset of the system
#define RESET_COUNTER_MAGIC 0xCAFEBABE

// Number of consecutive soft resets allowed by Error_Handler; after that it
// stops resetting and just reports over CAN
#define MAX_SOFT_RESETS 3

// Pause before resetting so error messages can go out (ms)
#define ERROR_HANDLER_DELAY_MS 100

// ============================================================================
// 3. FDCAN RECEIVE FILTER
// ============================================================================

// One mask filter (FilterID1 = filter, FilterID2 = mask). ID and mask of 0
// accept every standard-ID frame into RX FIFO0.
#define CAN_RX_FILTER_INDEX 0
#define CAN_RX_FILTER_ID 0x000
#define CAN_RX_FILTER_MASK 0x000

// ============================================================================
// 4. DEBUG CONSOLE
// ============================================================================

// Timeout for each character sent by printf (ms)
#define CONSOLE_TX_TIMEOUT_MS 0xFFFF

#endif  // CORE_CONFIG_H

/**
 * @file adbms_config.h
 * @brief Configuration for the ADBMS6830 pack monitoring layer
 *
 * Sections:
 *   1. Pack topology
 *   2. Safety thresholds and fault flags
 *   3. Cell balancing
 *   4. Measurement conversion
 *   5. Temperature sensor lookup
 *   6. Chip-select, SPI and timing
 *   7. ADBMS6830 unique IDs
 *
 * SOC estimator settings live in kalman_soc_config.h.
 */

#ifndef ADBMS_CONFIG_H
#define ADBMS_CONFIG_H

#include "comms_config.h"

// ============================================================================
// 1. PACK TOPOLOGY
// ============================================================================

#define TOTAL_IC 6
#define TOTAL_MODULES 6  // E.g., 12 ICs = 6 Segments
#define TOTAL_CELLS TOTAL_MODULES* CELLS_PER_MOD
#define TEMP_PER_BOARD 10
#define VOLT_PER_SLAVE 10
#define VOLT_PER_MASTER 14

// SOC CHANGES
#define CELL_TIE_TOLERANCE 0.0002f

// ============================================================================
// 2. SAFETY THRESHOLDS AND FAULT FLAGS
// ============================================================================

#define CELL_OV_THRESHOLD_V 4.25f  // Overvoltage limit
#define CELL_UV_THRESHOLD_V 2.5f   // Undervoltage limit
#define CELL_OT_THRESHOLD_C 60.0f  // Overtemperature limit (Celsius)

// Bits of SegmentData_t.fault_flags
#define FAULT_FLAG_OV 0x01            // Cell overvoltage
#define FAULT_FLAG_UV 0x02            // Cell undervoltage
#define FAULT_FLAG_OT 0x04            // Overtemperature
#define FAULT_FLAG_COMM 0x08          // Communication dropped
#define FAULT_FLAG_TEMP_SENSORS 0x10  // Too few working temperature sensors

// Consecutive bad cycles before a communication fault is latched
#define COMM_FAULT_LATCH 3  // consecutive bad cycles before hard fault
// Saturation value for the per-segment bad-cycle counter
#define COMM_MISS_COUNT_MAX 255

// A segment faults when (working sensor fraction * TEMP_COVERAGE_FACTOR) < MIN_CELL_THRESH
#define MIN_CELL_THRESH 0.2
#define TEMP_COVERAGE_FACTOR 0.5
// Cells lost per dead temperature sensor when checking coverage
#define TEMP_DROPOUT_DEAD_CELLS 2

// ============================================================================
// 3. CELL BALANCING
// ============================================================================

// Balance a cell if it is this far above the weakest cell in its segment
#define BAL_HYSTERESIS_V 0.05f  // 5mV hysteresis
// Cells at or below this voltage (mV) are treated as empty channels and ignored
#define BAL_VALID_CELL_MIN_MV 1000
// Starting value for the minimum-voltage search (mV)
#define BAL_MIN_SEARCH_INIT_MV 9999

// A segment's discharge mask is split across its two ICs:
// the master IC drives the first cells, the slave IC the rest
#define BAL_MASTER_DCC_MASK 0x3FFF  // Bits 0-13
#define BAL_SLAVE_DCC_SHIFT 14
#define BAL_SLAVE_DCC_MASK 0x03FF  // Bits 14-23 shifted down

// ============================================================================
// 4. MEASUREMENT CONVERSION
// ============================================================================

#define ADBMS_VOLTAGE_LSB_V 0.00015f  // ADBMS6830 ADC LSB is 150uV
// ADC codes are offset from zero by this voltage
#define ADBMS_CODE_OFFSET_V 1.5f
// The first and last cell of a segment read this much low; added back
#define ADBMS_END_CELL_OFFSET_V 0.1f
// Cells at or below this voltage (V) are not checked against OV/UV limits
#define CELL_FAULT_CHECK_MIN_V 1.0f

// ============================================================================
// 5. TEMPERATURE SENSOR LOOKUP (Enepaq VTC5A)
// ============================================================================

#define SENSOR_DROPOUT_TEMP -99
// Reported when a sensor reads as disconnected or invalid
#define TEMP_SENSOR_FAULT_C -99.0f

// Maps voltages to temperatures from -40C to +120C in 5C steps
#define TEMP_TABLE_SIZE 33
#define TEMP_TABLE_MIN_C -40.0f
#define TEMP_TABLE_MAX_C 120.0f
#define TEMP_TABLE_STEP_C 5.0f

// Above this voltage (V) the sensor is probably floating / disconnected
#define TEMP_FLOATING_THRESHOLD_V 2.8f

#define TEMP_TABLE_V_DATA                                                            \
    {                                                                                \
        2.44, 2.42, 2.40, 2.38, 2.35, 2.32, 2.27, 2.23, 2.17, 2.11, /* -40 to 5   */ \
        2.05, 1.99, 1.92, 1.86, 1.80, 1.74, 1.68, 1.63, 1.59, 1.55, /* 10 to 55  */  \
        1.51, 1.48, 1.45, 1.43, 1.40, 1.38, 1.37, 1.35, 1.34, 1.33, /* 60 to 105 */  \
        1.32, 1.31, 1.30                                            /* 110 to 120 */ \
    }

// ============================================================================
// 6. CHIP-SELECT, SPI AND TIMING
// ============================================================================

#define CS_PIN GPIO_PIN_6 /* Mcu dependent chip select */
#define GPIO_PORT GPIOB   /* Mcu dependent adc chip select port */

#define SPI_TIME_OUT 500            /* SPI Time out delay   */
#define UART_TIME_OUT HAL_MAX_DELAY /* UART Time out delay  */
#define I2C_TIME_OUT HAL_MAX_DELAY  /* I2C Time out delay   */

// Bytes of command and PEC that precede the data on every SPI transaction
#define ADBMS_CMD_HEADER_BYTES 4

#define WAKEUP_DELAY 1 /* BMS ic wakeup delay  */

// Time for the GPIO ADC conversions to finish (ms)
#define ADBMS_GPIO_CONVERSION_DELAY_MS 5
// Time for the ADC filter to settle after starting continuous conversion (ms)
#define ADBMS_ADC_SETTLE_DELAY_MS 8

// Configuration register defaults
#define ADBMS_GPO_DEFAULT 0X3FF
// Balancing PWM registers
#define ADBMS_PWM_DATA_BYTES 6
#define ADBMS_PWMA_DEFAULT_DATA {0x44, 0x44, 0x44, 0x44, 0x44, 0x44}  // Cells 1-8 at 50%
#define ADBMS_PWMB_DEFAULT_DATA \
    {0x44, 0x44, 0x44, 0x44, 0x44, 0x44}  // Cells 9-10 at 50%, 11-16 at 0%

// ============================================================================
// 7. ADBMS6830 UNIQUE IDS
// ============================================================================

// Unique 48 Bit IDs. IDs 1-7 are the top boards, 8-14 the bottom boards
#define ADBMS_TOP_BOARD_ID_MAX 7
#define ID1_TOP 0x97DE40006B21ULL
#define ID2_TOP 0x997CE40586B2ULL
#define ID3_TOP 0x097EE40006B2ULL
#define ID4_TOP 0x990DE40686B2ULL
#define ID5_TOP 0xC97DE40506B2ULL
#define ID6_TOP 0x590EE40586B2ULL
#define ID7_TOP 0x9A06E40606B2ULL
#define ID1_BOTTOM 0x0001111111ULL
#define ID2_BOTTOM 0x0002111111ULL
#define ID3_BOTTOM 0x0003111111ULL
#define ID4_BOTTOM 0x0004111111ULL
#define ID5_BOTTOM 0x0005111111ULL
#define ID6_BOTTOM 0x0006111111ULL
#define ID7_BOTTOM 0x0001111111ULL

#endif  // ADBMS_CONFIG_H

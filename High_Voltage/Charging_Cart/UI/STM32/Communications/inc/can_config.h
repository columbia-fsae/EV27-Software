/**
 * @file can_config.h
 * @brief Configuration for the charging cart CAN interface
 *
 * Sections:
 *   1. Pack topology
 *   2. Error bits
 *   3. Message IDs
 *   4. Message layout and decoding
 *   5. Charger (Elcon) commands
 *
 * The receive layout below has to match what the tractive battery sends, see
 * comms_config.h in High_Voltage/Tractive_Battery.
 */

#ifndef CAN_CONFIG_H
#define CAN_CONFIG_H

// ============================================================================
// 1. PACK TOPOLOGY
// ============================================================================

#define TOTAL_IC 12
#define TOTAL_SEGMENTS (TOTAL_IC / 2)  // E.g., 12 ICs = 6 Segments
#define TOTAL_MODULES 144
#define MOD_PER_SEG 24
#define MAX_CURRENT 250.0f

// ============================================================================
// 2. ERROR BITS
// ============================================================================

// Send Errors
#define CHARGER_TO_PACK_ERROR 0
// Receive Errors
#define CAN_RECEPTION_ERROR 0
#define CAN_NOTIFICATION_ERROR 1
#define ADC_ERROR 2
// Init and STM Errors
#define INIT_CAN_SEND_ERROR 0
#define INIT_ERROR 1

// ============================================================================
// 3. MESSAGE IDS
// ============================================================================

#define CAN_ID_BSM 2
#define CAN_ID_BMS_DATA_START 200
#define CAN_ID_BMS_DATA_END 235
#define CAN_ID_BATTERY_PACK 236
#define CAN_ID_BMS_STATS 237
#define CAN_ID_BMS_AVGS 238
#define CAN_ID_SOC_PACK 240
#define CAN_ID_SOC_DATA_START 241
#define CAN_ID_SOC_DATA_END 258
#define CAN_ID_CHARGING_TO_ELCON 0x1806E7F4  // These should either be E5, E7, E8, or E9
#define CAN_ID_ELCON_TO_CHARGING 0x18FF50E7
#define CAN_CHARGING_TO_PACK 20
#define CAN_ID_CHARGER_ERRORS 21

// ============================================================================
// 4. MESSAGE LAYOUT AND DECODING
// ============================================================================

// Cell voltage/temperature messages: cells per message and messages per segment
#define BMS_DATA_CELLS_PER_MSG 4
#define BMS_DATA_MSGS_PER_SEGMENT 6
// SOC messages
#define SOC_DATA_CELLS_PER_MSG 8
#define SOC_DATA_MSGS_PER_SEGMENT 3

// Elcon status message needs at least voltage (2), current (2) and status (1) bytes
#define ELCON_RX_MIN_DLC 5

// Message payload lengths sent by the cart
#define BATTERY_MSG_DLC 3
#define ERROR_MSG_DLC 3
#define ELCON_MSG_DLC 8

// Nominal cell voltage byte from pack voltage: (packVoltage / divisor) - offset
#define NOM_VOLT_DIVISOR 60
#define NOM_VOLT_OFFSET 180

// Raw value conversions (see the conversion functions in can.c for units)
#define CELL_V_STEP_MV 10       // raw cell voltage count -> mV
#define CELL_V_OFFSET_MV 1800   // mV added to the scaled cell voltage
#define CELL_TEMP_STEP 25       // raw cell temperature count -> 0.01 C
#define CELL_SOC_RAW_MAX 255    // raw cell SOC full scale
#define PACK_V_DIVISOR 10       // raw pack voltage -> 0.1 V
#define PACK_SOC_RAW_MAX 65535  // raw pack SOC full scale

// ============================================================================
// 5. CHARGER (ELCON) COMMANDS
// ============================================================================

// User voltage limit is in volts; the charger takes deci-volts
#define CHARGER_V_LIMIT_SCALE 10

#endif  // CAN_CONFIG_H

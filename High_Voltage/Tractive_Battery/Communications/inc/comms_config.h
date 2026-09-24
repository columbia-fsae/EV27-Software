/**
 * @file comms_config.h
 * @brief Configuration for the Communications layer (CAN, ADC, GPIO) and
 *        the pack geometry shared by every other module
 *
 * Sections:
 *   1. Pack geometry
 *   2. CAN bit timing
 *   3. CAN message IDs and error bits
 *   4. CAN signal encoding (scales, offsets, packing)
 *   5. ADC scaling and temperature sensor data
 *   6. GPIO pin mapping and polarity
 *
 * Pin macros refer to the CubeMX-generated names in main.h, so the .ioc stays
 * the single source of truth for which physical pin does what.
 */

#ifndef COMMS_CONFIG_H
#define COMMS_CONFIG_H

// ============================================================================
// 1. PACK GEOMETRY
// ============================================================================

#define CELLS_PER_MOD 10
#define TEMP_PER_MOD 10
// Temperature sensors stored per segment (two ADBMS boards x 10 sensors)
#define TEMPS_PER_SEGMENT 20

// ============================================================================
// 2. CAN BIT TIMING
// ============================================================================

// CAN DEFINITIONS for 1 MBPS (Car) for 250 KBPS (Charger)
#define CAN_CAR_Nominal_Prescaler 1
#define CAN_CAR_Nominal_TSeg_1 136
#define CAN_CAR_Nominal_TSeg_2 33
#define CAN_CAR_Nominal_SJW 33
#define CAN_CAR_Data_Prescaler 17
#define CAN_CAR_Data_SJW 3
#define CAN_CAR_Data_TSeg_1 6
#define CAN_CAR_Data_TSeg_2 3

#define CAN_Charger_Nominal_Prescaler 5
#define CAN_Charger_Nominal_TSeg_1 108
#define CAN_Charger_Nominal_TSeg_2 27
#define CAN_Charger_Nominal_SJW 27
#define CAN_Charger_Data_Prescaler 17
#define CAN_Charger_Data_SJW 16
#define CAN_Charger_Data_TSeg_1 23
#define CAN_Charger_Data_TSeg_2 16

// ============================================================================
// 3. CAN MESSAGE IDS AND ERROR BITS
// ============================================================================

// CAN Send Errors Bits
#define BMS_CAN_ERROR 0
#define BMS_CAN_PACK_ERROR 1
#define BSM_CAN_ERROR 2
#define SOC_CAN_PACK_ERROR 3
#define SOC_CAN_ERROR 4
#define PACK_SENSE_ERROR 5
#define BMS_CAN_STATS_ERROR 6
#define BMS_CAN_IDS_ERROR 7

// CAN Receive Errors Bits
#define CAN_RECEPTION_ERROR 0
#define CAN_NOTIFICATION_ERROR 1
#define ADC_ERROR 2

// CAN Init and STM Errors Bits
#define INIT_CAN_SEND_ERROR 0
// INIT Error
#define INIT_ERROR 1

// CAN IDS
#define CAN_ID_BSM 2
#define CAN_ID_PACK_SENSE 6
#define CAN_ID_ERRORS 10
#define CAN_ID_CHARGER 20
#define CAN_ID_INVERTER_CURRENT 166
#define CAN_ID_INVERTER_VOLTAGE 167
#define CAN_ID_BMS_INIT 200
#define CAN_ID_BMS_PACK 236
#define CAN_ID_BMS_STATS 237
#define CAN_ID_BMS_IDS 238
#define CAN_ID_SOC_INIT 240
#define CAN_ID_ELCON_CURRENT 0x18FF50E7

// ============================================================================
// 4. CAN SIGNAL ENCODING
// ============================================================================

// Received signal scaling (raw counts -> engineering units)
#define CAN_RX_INVERTER_CURRENT_SCALE 0.1f
#define CAN_RX_INVERTER_VOLTAGE_SCALE 0.1f
#define CAN_RX_ELCON_CURRENT_SCALE 0.1f

// Full-scale limits used when clamping a value into a CAN field
#define CAN_U8_MAX 255.0f
#define CAN_U16_MAX 65535.0f
#define CAN_U12_MAX 4095

// Cell voltage byte: (V - offset) / scale
#define CAN_CELL_V_OFFSET 1.8
#define CAN_CELL_V_SCALE 0.01
// Temperature byte: degC * scale
#define CAN_TEMP_SCALE 4.0f
// Total pack voltage (BMS pack message): V / scale, 16 bit
#define CAN_PACK_V_SCALE 0.01f

// ADC CAN Scalars
#define PACK_VOLTAGE_SCALE 0.25f  // (600.0f-330.0f)/255.0f
#define TSENSE_SCALE 0.5f
#define TSENSE_OFFSET (-10.0f)

// Cell voltage / temperature message packing: cells carried per message and
// messages used per segment (drives the ID offset from CAN_ID_BMS_INIT)
#define BMS_CAN_CELLS_PER_MSG 4
#define BMS_CAN_MSGS_PER_SEGMENT 6

// SOC message packing, same idea (offset from CAN_ID_SOC_INIT + 1)
#define SOC_CAN_CELLS_PER_MSG 8
#define SOC_CAN_MSGS_PER_SEGMENT 3

// Pack capacity (Ah x cells) that maps to full scale in the SOC pack message
#define CAN_SOC_CAPACITY_FULLSCALE (10.4 * 144)

// ============================================================================
// 5. ADC SCALING AND TEMPERATURE SENSOR DATA
// ============================================================================

// Raw DMA buffer lengths (channels converted per ADC)
#define ADC1_CHANNEL_COUNT 5
#define ADC2_CHANNEL_COUNT 1
// How long to wait for ADC2 to report ready when started by hand (ms)
#define ADC2_ENABLE_TIMEOUT_MS 200

// ADC Voltage and Temperature Conversion Scalars
#define ADC_VSENSE_SCALAR 0.24216f
#define ADC_LSB_VOLTAGE (3.3f / 4095.0f)
#define ADC_TSENSE_RPU 10000.0f
#define ADC_TSENSE_R25 10000.0f
#define ADC_TSENSE_VREF 3.3f
#define ADC_TSENSE_LOOKUP_SIZE 27

// Temperature ADC sensor lookup data: T (degC), R(T)/R(25)
#define ADC_TSENSE_LOOKUP_DATA                                                                     \
    {{-10, 4.651},   {-5, 3.663},    {0, 2.905},    {5, 2.319},    {10, 1.862},    {15, 1.505},    \
     {20, 1.223},    {25, 1.0},      {30, 0.8219},  {35, 0.6792},  {40, 0.5641},   {45, 0.4708},   \
     {50, 0.3949},   {55, 0.3327},   {60, 0.2816},  {65, 0.2393},  {70, 0.2043},   {75, 0.1751},   \
     {80, 0.1506},   {85, 0.1301},   {90, 0.1128},  {95, 0.09811}, {100, 0.08564}, {105, 0.07501}, \
     {110, 0.06591}, {115, 0.05809}, {120, 0.05136}}

// ============================================================================
// 6. GPIO PIN MAPPING AND POLARITY
// ============================================================================

// Inputs
#define GPIO_IR_PLUS_AUX_PORT IR_Plus_GPIO_Port
#define GPIO_IR_PLUS_AUX_PIN IR_Plus_Pin
#define GPIO_IR_MINUS_AUX_PORT IR_Minus_GPIO_Port
#define GPIO_IR_MINUS_AUX_PIN IR_Minus_Pin
#define GPIO_SLOW_CAN_PORT Slow_CAN_GPIO_Port
#define GPIO_SLOW_CAN_PIN Slow_CAN_Pin
#define GPIO_SDC_OK_PORT SDC_GPIO_Port
#define GPIO_SDC_OK_PIN SDC_Pin
#define GPIO_MCU_MHS_PORT MCU_MHS_GPIO_Port
#define GPIO_MCU_MHS_PIN MCU_MHS_Pin
#define GPIO_MCU_MLS_PORT MCU_MLS_GPIO_Port
#define GPIO_MCU_MLS_PIN MCU_MLS_Pin

// Outputs
#define GPIO_IR_PLUS_EN_PORT IR_Plus_EN_GPIO_Port
#define GPIO_IR_PLUS_EN_PIN IR_Plus_EN_Pin
#define GPIO_IR_MINUS_EN_PORT IR_Minus_EN_GPIO_Port
#define GPIO_IR_MINUS_EN_PIN IR_Minus_EN_Pin
#define GPIO_PRECHARGE_EN_PORT Precharge_EN_GPIO_Port
#define GPIO_PRECHARGE_EN_PIN Precharge_EN_Pin
#define GPIO_BMS_OK_PORT BMS_OK_GPIO_Port
#define GPIO_BMS_OK_PIN BMS_OK_Pin

// Polarity: 1 = signal is inverted at the pin
#define IR_PLUS_AUX_INVERTING 1
#define IR_MINUS_AUX_INVERTING 1
#define SLOW_CAN_INVERTING 1
#define SDC_OK_INVERTING 1
#define MCU_MHS_INVERTING 1
#define MCU_MLS_INVERTING 1
#define IR_PLUS_EN_INVERTING 0
#define IR_MINUS_EN_INVERTING 0
#define PRECHARGE_EN_INVERTING 0

#endif  // COMMS_CONFIG_H

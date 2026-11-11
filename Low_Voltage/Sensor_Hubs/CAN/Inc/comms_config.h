#ifndef COMMS_CONFIG_H
#define COMMS_CONFIG_H

// CAN IDs
#define CAN_ID_SHFL_1    ((uint16_t)100)
#define CAN_ID_SHFL_2    ((uint16_t)101)
#define CAN_ID_SHFR_1    ((uint16_t)102)
#define CAN_ID_SHFR_2    ((uint16_t)103)
#define CAN_ID_SHRL_1    ((uint16_t)104)
#define CAN_ID_SHRL_2    ((uint16_t)105)
#define CAN_ID_SHRR_1    ((uint16_t)106)
#define CAN_ID_SHRR_2    ((uint16_t)107)
#define CAN_ID_SHFL_DIAG ((uint16_t)108)
#define CAN_ID_SHFR_DIAG ((uint16_t)109)
#define CAN_ID_SHRL_DIAG ((uint16_t)110)
#define CAN_ID_SHRR_DIAG ((uint16_t)111)
#define CAN_ID_TEMP_FL_1 ((uint16_t)112)
#define CAN_ID_TEMP_FL_2 ((uint16_t)113)
#define CAN_ID_TEMP_FL_3 ((uint16_t)114)
#define CAN_ID_TEMP_FL_4 ((uint16_t)115)
#define CAN_ID_TEMP_FR_1 ((uint16_t)116)
#define CAN_ID_TEMP_FR_2 ((uint16_t)117)
#define CAN_ID_TEMP_FR_3 ((uint16_t)118)
#define CAN_ID_TEMP_FR_4 ((uint16_t)119)
#define CAN_ID_TEMP_RL_1 ((uint16_t)120)
#define CAN_ID_TEMP_RL_2 ((uint16_t)121)
#define CAN_ID_TEMP_RL_3 ((uint16_t)122)
#define CAN_ID_TEMP_RL_4 ((uint16_t)123)
#define CAN_ID_TEMP_RR_1 ((uint16_t)124)
#define CAN_ID_TEMP_RR_2 ((uint16_t)125)
#define CAN_ID_TEMP_RR_3 ((uint16_t)126)
#define CAN_ID_TEMP_RR_4 ((uint16_t)127)

// Data Scaling
// TODO: Should be refactored/renamed to acual sensor for clarify
#define ADC1_SCALE 1.0f
#define ADC2_SCALE 1.0f
#define ADC3_SCALE 1.0f
#define ADC4_SCALE 1.0f

#define SHUNT_VOLT_OFFSET   0.32768F  // 0.16384 * 2
#define SHUNT_VOLT_SCALE    -0.16384F
#define BUS_VOLT_OFFSET     17.5F
#define BUS_VOLT_SCALE      11.9F  // 29.4 - 17.5
#define CURRENT_SCALE       1.0F
#define TEMP_OFFSET         10.0F
#define TEMP_SCALE          50.0F  // 60 - 10
#define POWER_OFFSET        4.0F
#define POWER_SCALE         4.0F  // 8 - 4
#define STM_VOLT_MON_OFFSET 4.8F
#define STM_VOLT_MON_SCALE  0.4F  // 5.2 - 4.8

#define WSS_SCALE 150.0F

// CAN Send Errors Bits
#define SH_MSG_1_ERROR  0
#define SH_MSG_2_ERROR  1
#define SH_DIAG_ERROR   2
#define SH_TEMP_1_ERROR 3
#define SH_TEMP_2_ERROR 4
#define SH_TEMP_3_ERROR 5
#define SH_TEMP_4_ERROR 6
#define UNKNOWN_ERROR   0xFF

// CAN Receive Errors Bits
#define CAN_RECEPTION_ERROR         0
// CAN Init and STM Errors Bits
#define INIT_CAN_SEND_ERROR         1
// INIT Error
#define INIT_ERROR                  2
// CAN Queue Errors
#define QUEUE_CAN_SEND_ADD_FULL     3
#define QUEUE_CAN_SEND_POP_EMPTY    4
#define QUEUE_CAN_SEND_POP_ERROR    5
#define QUEUE_CAN_RECEIVE_ADD_FULL  6
#define QUEUE_CAN_RECEIVE_POP_ERROR 7

// ============================================================================
// 4. CAN SIGNAL ENCODING
// ============================================================================

// Full-scale limits used when clamping a value into a CAN field
#define CAN_U8_MAX  255.0f
#define CAN_U16_MAX 65535.0f
#define CAN_U12_MAX 4095

#define UINT8_LEN 8U

#define CAN_TX_QUEUE_LOW_PRIO  0
#define CAN_TX_QUEUE_NORM_PRIO 1
#define cAN_TX_QUEUE_MED_PRIO  2
#define CAN_TX_QUEUE_HIGH_PRIO 3

#define CAN_ID_TEMP_FL_IN1 ((uint16_t)0x4b0)
#define CAN_ID_TEMP_FL_IN2 ((uint16_t)0x4b2)
#define CAN_ID_TEMP_FL_IN3 ((uint16_t)0x4b2)
#define CAN_ID_TEMP_FL_IN4 ((uint16_t)0x4b3)
#define CAN_ID_TEMP_FR_IN1 ((uint16_t)0x4b4)
#define CAN_ID_TEMP_FR_IN2 ((uint16_t)0x4b5)
#define CAN_ID_TEMP_FR_IN3 ((uint16_t)0x4b6)
#define CAN_ID_TEMP_FR_IN4 ((uint16_t)0x4b7)
#define CAN_ID_TEMP_RL_IN1 ((uint16_t)0x4b8)
#define CAN_ID_TEMP_RL_IN2 ((uint16_t)0x4b9)
#define CAN_ID_TEMP_RL_IN3 ((uint16_t)0x4ba)
#define CAN_ID_TEMP_RL_IN4 ((uint16_t)0x4bb)
#define CAN_ID_TEMP_RR_IN1 ((uint16_t)0x4bc)
#define CAN_ID_TEMP_RR_IN2 ((uint16_t)0x4bd)
#define CAN_ID_TEMP_RR_IN3 ((uint16_t)0x4be)
#define CAN_ID_TEMP_RR_IN4 ((uint16_t)0x4bf)

#endif

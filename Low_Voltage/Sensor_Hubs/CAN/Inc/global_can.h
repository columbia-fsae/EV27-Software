#ifndef GLOBAL_CAN_H
#define GLOBAL_CAN_H

#include <stdbool.h>

#include "Common.h"

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

// CAN Error Handling Storage
typedef struct {
    uint8_t message_send_errors;
    uint8_t general_errors;
    uint32_t min_stack_size;
    uint8_t stack_overflow;
    uint32_t queue_num;
    uint32_t queue_space;
} Errors;

extern Errors error_info;

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

void CAN_SendData(osMessageQueueId_t* queue_can_tx_handle, FDCAN_HandleTypeDef* hfdcan1);
HAL_StatusTypeDef CAN_SendData_Init(uint16_t id_m, uint8_t* data, uint32_t length,
                                    FDCAN_HandleTypeDef* hfdcan1);

// Clamping Functions for CAN Sending
static inline uint8_t clamp_u8(float value, float min, float max) {
    if (value < min) value = min;
    if (value > max) value = max;
    return (uint8_t)value;
}
static inline int8_t clamp_i8(float value, float min, float max) {
    if (value < min) value = min;
    if (value > max) value = max;
    return (int8_t)value;
}
static inline uint16_t clamp_u16(float value, float min, float max) {
    if (value < min) value = min;
    if (value > max) value = max;
    return (uint16_t)value;
}
static inline int16_t clamp_i16(float value, float min, float max) {
    if (value < min) value = min;
    if (value > max) value = max;
    return (int16_t)value;
}

#endif

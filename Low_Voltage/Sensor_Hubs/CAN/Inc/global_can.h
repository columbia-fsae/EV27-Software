#ifndef GLOBAL_CAN_H
#define GLOBAL_CAN_H

#include <stdbool.h>

#include "Common.h"

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

void sensor1_can(Mutex_Struct_t* mutex_struct, osMessageQueueId_t* queue_can_tx_handle,
                 adc1_info* global_adc1, adc2_info* global_adc2);
void sensor2_can(Mutex_Struct_t* mutex_struct, osMessageQueueId_t* queue_can_tx_handle, float wss);
void diag_can(Mutex_Struct_t* mutex_struct, osMessageQueueId_t* queue_can_tx_handle,
              mon_i2c_data* mon_data, adc1_info* global_adc1);
void temps_can(Mutex_Struct_t* mutex_struct, osMessageQueueId_t* queue_can_tx_handle,
               struct temp (*temp_data)[16]);
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

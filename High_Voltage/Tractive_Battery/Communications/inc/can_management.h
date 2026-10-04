#ifndef CAN_MANAGEMENT_H
#define CAN_MANAGEMENT_H

#include <stdbool.h>

#include "adBms_Application.h"
#include "bsm.h"
#include "common_types.h"
#include "comms_config.h"
#include "gpio_management.h"
#include "main.h"

// CAN Error Handling Storage
typedef struct {
    uint8_t message_send_errors;
    uint8_t message_receive_errors;
    uint8_t message_init_send_errors;
} Errors;

extern Errors error_info;

void can_init(FDCAN_HandleTypeDef* hfdcan1, GPIO_Info_t* gpio);
void bms_can_stats(SegmentData_t* PackData, TotalPack_t* TotalPack, FDCAN_HandleTypeDef* hfdcan1);
void bms_can_faults(SegmentData_t* PackData, TotalPack_t* TotalPack, FDCAN_HandleTypeDef* hfdcan1);
void bms_can_data(SegmentData_t* PackData, TotalPack_t* TotalPack, FDCAN_HandleTypeDef* hfdcan1,
                  uint8_t* bms_mod_counter, uint8_t* bms_segment_counter);
void bsm_can(bsm_obj* bsm, GPIO_Info_t* gpio_data, FDCAN_HandleTypeDef* hfdcan1);
void soc_can_stats(SegmentData_t* PackData, SOC_Estimate soc[][CELLS_PER_MOD], TotalPack_t* pack,
                   FDCAN_HandleTypeDef* hfdcan1);
void bms_can_ids(SegmentData_t* PackData, SOC_Estimate soc[][CELLS_PER_MOD], TotalPack_t* pack,
                 FDCAN_HandleTypeDef* hfdcan1);
void soc_can_data(SOC_Estimate soc[][CELLS_PER_MOD], TotalPack_t* pack,
                  FDCAN_HandleTypeDef* hfdcan1, uint8_t* soc_mod_counter,
                  uint8_t* soc_segment_counter);
void error_can(FDCAN_HandleTypeDef* hfdcan1);
void adc_can(ADC_Inputs_t* adc_data, FDCAN_HandleTypeDef* hfdcan1);
HAL_StatusTypeDef CAN_SendData(uint16_t id, uint8_t* data, uint32_t length,
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

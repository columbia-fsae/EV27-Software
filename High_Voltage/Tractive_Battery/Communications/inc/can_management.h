#ifndef CAN_MANAGEMENT_H
#define CAN_MANAGEMENT_H

#include <stdbool.h>

#include "adBms_Application.h"
#include "bsm.h"
#include "common_types.h"
#include "gpio_management.h"
#include "main.h"

// CAN Error Handling Storage
typedef struct {
    uint8_t message_send_errors;
    uint8_t message_receive_errors;
    uint8_t message_init_send_errors;
    uint8_t can_queue_errors;
    uint8_t queue_info;
} Errors;

extern Errors error_info;

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
// UNUSED:
#define CAN_NOTIFICATION_ERROR 1
#define ADC_ERROR 2

// CAN Init and STM Errors Bits
#define INIT_CAN_SEND_ERROR 0
// INIT Error
#define INIT_ERROR 1

// CAN Queue Errors
#define QUEUE_CAN_SEND_ADD_FULL 0
#define QUEUE_CAN_SEND_POP_EMPTY 1
#define QUEUE_CAN_SEND_POP_ERROR 2
#define QUEUE_CAN_REC_ADD_FULL 3
#define QUEUE_CAN_REC_POP_EMPTY 4
#define QUEUE_CAN_REC_POP_ERROR 5

// CAN IDS
#define CAN_ID_BSM ((uint16_t)2)
#define CAN_ID_PACK_SENSE ((uint16_t)6)
#define CAN_ID_ERRORS ((uint16_t)10)
#define CAN_ID_CHARGER ((uint16_t)20)
#define CAN_ID_INVERTER_CURRENT ((uint16_t)166)
#define CAN_ID_INVERTER_VOLTAGE ((uint16_t)167)
#define CAN_ID_BMS_INIT ((uint16_t)200)
#define CAN_ID_BMS_PACK ((uint16_t)236)
#define CAN_ID_BMS_STATS ((uint16_t)237)
#define CAN_ID_BMS_IDS ((uint16_t)238)
#define CAN_ID_SOC_STATS ((uint16_t)240)
#define CAN_ID_SOC_INIT ((uint16_t)241)
#define CAN_ID_ELCON_CURRENT 0x18FF50E7

// ADC CAN Scalars
#define PACK_VOLTAGE_SCALE 0.25f  // (600.0f-330.0f)/255.0f
#define TSENSE_SCALE 0.5f
#define TSENSE_OFFSET (-10.0f)

void can_init(FDCAN_HandleTypeDef* hfdcan1, GPIO_Info_t* gpio);
void bms_can_stats(SegmentData_t* PackData, osMessageQueueId_t* Queue_CAN_TxHandle,
                   Mutex_Struct_t* mutex_struct);
void bms_can_faults(SegmentData_t* PackData, TotalPack_t* TotalPack,
                    osMessageQueueId_t* Queue_CAN_TxHandle, Mutex_Struct_t* mutex_struct);
void bms_can_data(SegmentData_t* PackData, uint8_t* bms_mod_counter, uint8_t* bms_segment_counter,
                  osMessageQueueId_t* Queue_CAN_TxHandle, Mutex_Struct_t* mutex_struct);
void bsm_can(bsm_obj* bsm, GPIO_Info_t* gpio_data, osMessageQueueId_t* Queue_CAN_TxHandle,
             Mutex_Struct_t* mutex_struct);
void soc_can_stats(TotalPack_t* pack, osMessageQueueId_t* Queue_CAN_TxHandle,
                   Mutex_Struct_t* mutex_struct);
void bms_can_ids(SegmentData_t* PackData, TotalPack_t* pack, osMessageQueueId_t* Queue_CAN_TxHandle,
                 Mutex_Struct_t* mutex_struct);
void soc_can_data(SOC_Estimate soc[][CELLS_PER_MOD], uint8_t* soc_mod_counter,
                  uint8_t* soc_segment_counter, osMessageQueueId_t* Queue_CAN_TxHandle,
                  Mutex_Struct_t* mutex_struct);
void error_can(osMessageQueueId_t* Queue_CAN_TxHandle, Mutex_Struct_t* mutex_struct);
void adc_can(ADC_Inputs_t* adc_data, osMessageQueueId_t* Queue_CAN_TxHandle,
             Mutex_Struct_t* mutex_struct);
void CAN_SendData(osMessageQueueId_t* Queue_CAN_TxHandle, FDCAN_HandleTypeDef* hfdcan1);

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

static uint8_t msg_to_error_bit(uint16_t id) {
    if (id == CAN_ID_BSM) {
        return (uint8_t)BSM_CAN_ERROR;
    } else if (id == CAN_ID_BMS_PACK) {
        return (uint8_t)BMS_CAN_PACK_ERROR;
    } else if (id == CAN_ID_BMS_STATS) {
        return (uint8_t)BMS_CAN_STATS_ERROR;
    } else if (id == CAN_ID_BMS_IDS) {
        return (uint8_t)BMS_CAN_IDS_ERROR;
    } else if (id >= CAN_ID_BMS_INIT && id < CAN_ID_BMS_PACK) {
        return (uint8_t)BMS_CAN_ERROR;
    } else if (id == CAN_ID_SOC_STATS) {
        return (uint8_t)SOC_CAN_PACK_ERROR;
    } else if (id >= CAN_ID_SOC_INIT && id < 300) {
        return (uint8_t)SOC_CAN_ERROR;
    } else if (id == CAN_ID_PACK_SENSE) {
        return (uint8_t)PACK_SENSE_ERROR;
    } else {
        return (uint8_t)0;
    }
}
#endif

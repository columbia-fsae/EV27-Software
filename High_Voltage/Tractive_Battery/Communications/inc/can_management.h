#ifndef CAN_MANAGEMENT_H
#define CAN_MANAGEMENT_H

#include "main.h"
#include "gpio_management.h"
#include "bsm.h"
#include "adBms_Application.h"
#include "common_types.h"
#include <stdbool.h>

//CAN Error Handling Storage
typedef struct{
	uint8_t message_send_errors;
	uint8_t message_receive_errors;
	uint8_t message_init_send_errors;
} Errors;

extern Errors error_info;

//CAN Send Errors Bits
#define BMS_CAN_ERROR 			0
#define BMS_CAN_PACK_ERROR 		1
#define BSM_CAN_ERROR 			2
#define SOC_CAN_PACK_ERROR 		3
#define SOC_CAN_ERROR 			4
#define PACK_SENSE_ERROR		5
#define BMS_CAN_STATS_ERROR		6
#define BMS_CAN_IDS_ERROR		7

//CAN Receive Errors Bits
#define CAN_RECEPTION_ERROR 	0
#define CAN_NOTIFICATION_ERROR  1
#define ADC_ERROR 				2

//CAN Init and STM Errors Bits
#define INIT_CAN_SEND_ERROR 0
//INIT Error
#define INIT_ERROR			1

//CAN IDS
#define CAN_ID_BSM 				2
#define CAN_ID_PACK_SENSE		6
#define CAN_ID_ERRORS 			10
#define CAN_ID_CHARGER          20
#define CAN_ID_INVERTER_CURRENT 166
#define CAN_ID_INVERTER_VOLTAGE 167
#define CAN_ID_BMS_INIT     	200
#define CAN_ID_BMS_PACK			236
#define CAN_ID_BMS_STATS		237
#define CAN_ID_BMS_IDS			238
#define CAN_ID_SOC_INIT			240
#define CAN_ID_ELCON_CURRENT 	0x18FF50E7

//ADC CAN Scalars
#define PACK_VOLTAGE_SCALE 0.25f // (600.0f-330.0f)/255.0f
#define TSENSE_SCALE 0.5f
#define TSENSE_OFFSET (-10.0f)

void can_init(FDCAN_HandleTypeDef *hfdcan1,GPIO_Info_t *gpio);
void bms_can_stats(SegmentData_t *PackData, TotalPack_t *TotalPack, FDCAN_HandleTypeDef *hfdcan1);
void bms_can_faults(SegmentData_t *PackData, TotalPack_t *TotalPack, FDCAN_HandleTypeDef *hfdcan1);
void bms_can_data(SegmentData_t *PackData, TotalPack_t *TotalPack, FDCAN_HandleTypeDef *hfdcan1, uint8_t *bms_mod_counter, uint8_t *bms_segment_counter);
void bsm_can(bsm_obj *bsm,GPIO_Info_t *gpio_data, FDCAN_HandleTypeDef *hfdcan1);
void soc_can_stats(SegmentData_t *PackData, SOC_Estimate soc[][CELLS_PER_MOD],TotalPack_t *pack,FDCAN_HandleTypeDef *hfdcan1);
void bms_can_ids(SegmentData_t *PackData, SOC_Estimate soc[][CELLS_PER_MOD],TotalPack_t *pack,FDCAN_HandleTypeDef *hfdcan1);
void soc_can_data(SOC_Estimate soc[][CELLS_PER_MOD],TotalPack_t *pack,FDCAN_HandleTypeDef *hfdcan1, uint8_t *soc_mod_counter, uint8_t *soc_segment_counter);
void error_can(FDCAN_HandleTypeDef *hfdcan1);
void adc_can(ADC_Inputs_t *adc_data, FDCAN_HandleTypeDef *hfdcan1);
HAL_StatusTypeDef CAN_SendData(uint16_t id, uint8_t *data, uint32_t length, FDCAN_HandleTypeDef *hfdcan1);

//Clamping Functions for CAN Sending
static inline uint8_t clamp_u8(float value, float min, float max){
	if (value < min) value = min;
	if (value > max) value = max;
	return (uint8_t)value;
}
static inline int8_t clamp_i8(float value, float min, float max){
	if (value < min) value = min;
	if (value > max) value = max;
	return (int8_t)value;
}
static inline uint16_t clamp_u16(float value, float min, float max){
	if (value < min) value = min;
	if (value > max) value = max;
	return (uint16_t)value;
}
static inline int16_t clamp_i16(float value, float min, float max){
	if (value < min) value = min;
	if (value > max) value = max;
	return (int16_t)value;
}
#endif

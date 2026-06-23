#ifndef CAN_H
#define CAN_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#include "stm32f0xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif


#define TOTAL_IC 12
#define TOTAL_SEGMENTS (TOTAL_IC / 2) // E.g., 12 ICs = 6 Segments
#define TOTAL_MODULES 144
#define MOD_PER_SEG 24
#define MAX_CURRENT 250.0f

//Send Errors
#define CHARGER_TO_PACK_ERROR	0

//Receive Errors
#define CAN_RECEPTION_ERROR 	0
#define CAN_NOTIFICATION_ERROR  1
#define ADC_ERROR 				2

//Init and STM Errors
#define INIT_CAN_SEND_ERROR     0
#define INIT_ERROR              1

//CAN IDS
#define CAN_ID_BSM 				2

#define CAN_ID_BMS_DATA_START   200
#define CAN_ID_BMS_DATA_END     235

#define CAN_ID_BATTERY_PACK 	236
#define CAN_ID_BMS_STATS		237
#define CAN_ID_BMS_AVGS         238
#define CAN_ID_SOC_PACK			240

#define CAN_ID_SOC_DATA_START   241
#define CAN_ID_SOC_DATA_END     258

#define CAN_ID_CHARGING_TO_ELCON 0x1806E7F4 //These should either be E5, E7, E8, or E9
#define CAN_ID_ELCON_TO_CHARGING 0x18FF50E7

#define CAN_CHARGING_TO_PACK 	20
#define CAN_ID_CHARGER_ERRORS 	21

typedef struct {
    volatile uint8_t maxTempVal,  minTempVal,  maxVoltVal,  minVoltVal, nomTempVal, nomVoltVal;
    volatile uint8_t  maxTempCell, minTempCell, maxVoltCell, minVoltCell;
    volatile uint16_t SOC, packVoltage;
    volatile bool balancingDone;
    volatile uint8_t bsmState;
    volatile uint16_t elconVoltage, elconCurrent;
    volatile uint8_t elconStatus;
    volatile uint32_t lastBSMUpdateTick, lastElconUpdateTick;
} CANInfo;

extern CANInfo CAN_Info;

/* --- GLOBAL PACK STATUS --- */
typedef struct {
    uint8_t cell_v[MOD_PER_SEG];  // Voltages scaled to millivolts (e.g., 4125 = 4.125V)
    int8_t   temp_C[MOD_PER_SEG];     // Temperatures in Celsius (e.g., 45 = 45C)
    uint8_t soc[MOD_PER_SEG];       // State of Charge in percentage (e.g., 85 = 85%)
} SegmentData_t;

typedef struct{
	volatile uint8_t message_send_errors;
	volatile uint8_t message_receive_errors;
	volatile uint8_t message_init_send_errors;
} Errors;

extern Errors CAN_error_info;

void can_init(CAN_HandleTypeDef *hcan);
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);


bool error_can(CAN_HandleTypeDef *hcan);
bool battery_can(CAN_HandleTypeDef *hcan, uint16_t current, bool balancing);
bool charger_can(CAN_HandleTypeDef *hcan, uint16_t v_lim, uint16_t i_lim, bool charge);

HAL_StatusTypeDef CAN_SendData(CAN_HandleTypeDef *hcan, uint32_t id, uint8_t *data, uint32_t length, bool ext);

uint16_t cell_voltage_conversion(uint8_t raw_value);
uint16_t cell_temperature_conversion(int8_t raw_value);
uint16_t cell_soc_conversion(uint8_t raw_value);

uint16_t pack_voltage_conversion(uint16_t raw_value);
uint16_t pack_soc_conversion(uint16_t raw_value);

#ifdef __cplusplus
}   
#endif

#endif

#ifndef CAN_H
#define CAN_H

#include <stdbool.h>
#include <stdint.h>

#include "can_config.h"
#include "main.h"
#include "stm32f0xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile uint8_t maxTempVal, minTempVal, maxVoltVal, minVoltVal, nomTempVal, nomVoltVal;
    volatile uint8_t maxTempCell, minTempCell, maxVoltCell, minVoltCell;
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
    int8_t temp_C[MOD_PER_SEG];   // Temperatures in Celsius (e.g., 45 = 45C)
    uint8_t soc[MOD_PER_SEG];     // State of Charge in percentage (e.g., 85 = 85%)
} SegmentData_t;

typedef struct {
    volatile uint8_t message_send_errors;
    volatile uint8_t message_receive_errors;
    volatile uint8_t message_init_send_errors;
} Errors;

extern Errors CAN_error_info;

void can_init(CAN_HandleTypeDef* hcan);
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan);

bool error_can(CAN_HandleTypeDef* hcan);
bool battery_can(CAN_HandleTypeDef* hcan, uint16_t current, bool balancing);
bool charger_can(CAN_HandleTypeDef* hcan, uint16_t v_lim, uint16_t i_lim, bool charge);

HAL_StatusTypeDef CAN_SendData(CAN_HandleTypeDef* hcan, uint32_t id, uint8_t* data, uint32_t length,
                               bool ext);

uint16_t cell_voltage_conversion(uint8_t raw_value);
uint16_t cell_temperature_conversion(int8_t raw_value);
uint16_t cell_soc_conversion(uint8_t raw_value);

uint16_t pack_voltage_conversion(uint16_t raw_value);
uint16_t pack_soc_conversion(uint16_t raw_value);

#ifdef __cplusplus
}
#endif

#endif

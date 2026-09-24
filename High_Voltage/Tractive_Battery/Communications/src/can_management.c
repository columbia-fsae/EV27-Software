#include "can_management.h"

#include <math.h>
#include <stdint.h>

#include "adBms_Application.h"
#include "adc_management.h"
#include "cmsis_os2.h"
#include "common.h"
#include "common_types.h"
#include "freertos_mpool.h"
#include "main.h"
#include "stm32g4xx_hal_fdcan.h"

static uint8_t RxData[8];
volatile CAN_Inputs_t can_data = {0};
static FDCAN_RxHeaderTypeDef RxHeader;
Errors error_info = {0};

void can_init(FDCAN_HandleTypeDef* hfdcan1, GPIO_Info_t* gpio) {
    // Switch CAN Baud rate based on what the battery is plugged in to
    if (gpio->slow_CAN) {  // Charger
        hfdcan1->Init.NominalPrescaler = CAN_Charger_Nominal_Prescaler;
        hfdcan1->Init.NominalSyncJumpWidth = CAN_Charger_Nominal_SJW;
        hfdcan1->Init.NominalTimeSeg1 = CAN_Charger_Nominal_TSeg_1;
        hfdcan1->Init.NominalTimeSeg2 = CAN_Charger_Nominal_TSeg_2;
        hfdcan1->Init.DataPrescaler = CAN_Charger_Data_Prescaler;
        hfdcan1->Init.DataSyncJumpWidth = CAN_Charger_Data_SJW;
        hfdcan1->Init.DataTimeSeg1 = CAN_Charger_Data_TSeg_1;
        hfdcan1->Init.DataTimeSeg2 = CAN_Charger_Data_TSeg_2;
    } else {  // Car
        hfdcan1->Init.NominalPrescaler = CAN_CAR_Nominal_Prescaler;
        hfdcan1->Init.NominalSyncJumpWidth = CAN_CAR_Nominal_SJW;
        hfdcan1->Init.NominalTimeSeg1 = CAN_CAR_Nominal_TSeg_1;
        hfdcan1->Init.NominalTimeSeg2 = CAN_CAR_Nominal_TSeg_2;
        hfdcan1->Init.DataPrescaler = CAN_CAR_Data_Prescaler;
        hfdcan1->Init.DataSyncJumpWidth = CAN_CAR_Data_SJW;
        hfdcan1->Init.DataTimeSeg1 = CAN_CAR_Data_TSeg_1;
        hfdcan1->Init.DataTimeSeg2 = CAN_CAR_Data_TSeg_2;
    }

    // Re-Initialize FDCAN1
    if (HAL_FDCAN_Init(hfdcan1) != HAL_OK) {
        Error_Handler();
        return;
    }

    // Start FDCAN1
    if (HAL_FDCAN_Start(hfdcan1) != HAL_OK) {
        Error_Handler();
        return;
    }

    // Activate the Notification for new data in FIFO0 for FDCAN1
    if (HAL_FDCAN_ActivateNotification(hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        Error_Handler();
        return;
    }
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs,
                               Mutex_Struct_t* mutex_struct,
                               osMessageQueueId_t* Queue_CAN_TxHandle) {
    Errors local_error_info;

    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET) {
        // Retrieve Rx Messages from Rx FIFO0
        // printf("CAN message receive\r\n");
        // while(HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0){
        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK) {
            // Reception Error
            // printf("CAN GET Error\r\n");
            local_error_info.message_receive_errors |= (1 << CAN_RECEPTION_ERROR);
            return;
        } else {
            local_error_info.message_receive_errors &= ~(1 << CAN_RECEPTION_ERROR);
        }

        CAN_Inputs_t local_can_inputs;

        xQueuePeek((QueueHandle_t)Queue_CAN_TxHandle, &local_can_inputs, 0);
        // Switch Statement that reads CAN message ID to fill in correct information
        switch (RxHeader.Identifier) {
            case CAN_ID_CHARGER:
                local_can_inputs.balancing_enable = (RxData[0] & 0b1);
                // can_data.tractive_current = (float)((((uint16_t)RxData[2]) << 8) |
                // ((uint16_t)RxData[1])); //TODO: CHANGE
                break;
            case CAN_ID_INVERTER_CURRENT:
                local_can_inputs.tractive_current =
                    (float)((int16_t)((((uint16_t)RxData[7]) << 8) | ((uint16_t)RxData[6]))) * 0.1f;
                break;
            case CAN_ID_INVERTER_VOLTAGE:
                local_can_inputs.dc_bus_voltage =
                    (float)((((uint16_t)RxData[1]) << 8) | ((uint16_t)RxData[0])) * 0.1f;
                break;
            case CAN_ID_ELCON_CURRENT:
                local_can_inputs.tractive_current =
                    (float)((int16_t)((((uint16_t)RxData[2]) << 8) | ((uint16_t)RxData[3]))) * 0.1f;
                break;
            default:
                break;
        }
        if (osMessageQueueGetSpace(Queue_CAN_TxHandle) > 0) {
            osMessageQueuePut(Queue_CAN_TxHandle, &local_can_inputs, CAN_TX_QUEUE_NORM_PRIO, 0);
            bitwiseAndWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_REC_ADD_FULL);
        } else {
            bitwiseOrWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_REC_ADD_FULL);
        }
    }

    copyWithMutex(mutex_struct->error_info_key, &error_info.message_receive_errors,
                  &local_error_info.message_receive_errors);
}

void bms_can_faults(SegmentData_t* PackData, TotalPack_t* TotalPack,
                    osMessageQueueId_t* Queue_CAN_TxHandle, Mutex_Struct_t* mutex_struct) {
    can_msg local_msg;
    SegmentData_t local_pack_data[TOTAL_MODULES];
    TotalPack_t local_pack;

    copyWithMutex(mutex_struct->pack_segments_key, &local_pack_data, PackData);
    copyWithMutex(mutex_struct->total_pack_key, &local_pack, TotalPack);

    // Send total voltage and all BMS fault flags for all Modules
    uint16_t temp_totalVoltage = clamp_u16((local_pack->voltage) / 0.01f, 0.0f, 65535.0f);
    local_msg.data[0] = (uint8_t)(temp_totalVoltage & 0xFF);
    local_msg.data[1] = (uint8_t)(temp_totalVoltage >> 8U);
    local_msg.data[2] = local_pack_data[0].fault_flags | (local_pack->balancing_done << 8U);
    for (int cic = 1; cic < TOTAL_MODULES; cic++) {
        local_msg.data[cic + 2] = local_pack_data[cic].fault_flags;
    }

    local_msg.id = CAN_ID_BMS_PACK;
    local_msg.length = FDCAN_DLC_BYTES_8;
    if (osMessageQueueGetSpace(Queue_CAN_TxHandle) > 0) {
        osMessageQueuePut(Queue_CAN_TxHandle, &local_msg, CAN_TX_QUEUE_HIGH_PRIO, 0);
        bitwiseAndWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    } else {
        bitwiseOrWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    }
}

void bms_can_stats(SegmentData_t* PackData, osMessageQueueId_t* Queue_CAN_TxHandle,
                   Mutex_Struct_t* mutex_struct) {
    can_msg local_msg;
    SegmentData_t local_pack_data[TOTAL_MODULES];

    copyWithMutex(mutex_struct->pack_segments_key, &local_pack_data, PackData);

    uint8_t maxVoltageIdx = 0;
    uint8_t minVoltageIdx = 0;
    uint8_t maxTempIdx = 0;
    uint8_t minTempIdx = 0;
    uint32_t maxVoltage = 0;
    uint32_t minVoltage = 65535;
    int8_t maxTemp = -128;
    int8_t minTemp = 127;

    // Calculate min, max, temp and voltage and total voltage from BMS information
    for (int cic = 0; cic < TOTAL_MODULES; cic++) {
        for (int i = 0; i < CELLS_PER_MOD; i++) {
            uint32_t temp_idx = cell_to_temp_index(i);
            if (local_pack_data[cic].cell_v_mV[i] > maxVoltage) {
                maxVoltage = local_pack_data[cic].cell_v_mV[i];
                maxVoltageIdx = cic * CELLS_PER_MOD + i;
            }
            if (local_pack_data[cic].cell_v_mV[i] < minVoltage) {
                minVoltage = local_pack_data[cic].cell_v_mV[i];
                minVoltageIdx = cic * CELLS_PER_MOD + i;
            }
            if (local_pack_data[cic].temp_C[temp_idx] > maxTemp) {
                maxTemp = local_pack_data[cic].temp_C[temp_idx];
                maxTempIdx = cic * CELLS_PER_MOD + i;
            }
            if (local_pack_data[cic].temp_C[temp_idx] < minTemp) {
                minTemp = local_pack_data[cic].temp_C[temp_idx];
                minTempIdx = cic * CELLS_PER_MOD + i;
            }
        }
    }
    // Send min and max voltage and temp values and IDs in structure

    local_msg.data[0] = (uint8_t)clamp_u8((((maxVoltage / 1000.0f) - 1.8) / 0.01), 0.0f, 255.0f);
    local_msg.data[1] = (uint8_t)clamp_u8((((minVoltage / 1000.0f) - 1.8) / 0.01), 0.0f, 255.0f);
    local_msg.data[2] = (uint8_t)clamp_u8(maxTemp * 4.0f, 0.0f, 255.0f);
    local_msg.data[3] = (uint8_t)clamp_u8(minTemp * 4.0f, 0.0f, 255.0f);
    local_msg.data[4] = maxVoltageIdx;
    local_msg.data[5] = minVoltageIdx;
    local_msg.data[6] = maxTempIdx;
    local_msg.data[7] = minTempIdx;

    local_msg.id = (uint16_t)CAN_ID_BMS_STATS;
    local_msg.length = FDCAN_DLC_BYTES_8;

    if (osMessageQueueGetSpace(Queue_CAN_TxHandle) > 0) {
        osMessageQueuePut(Queue_CAN_TxHandle, &local_msg, CAN_TX_QUEUE_NORM_PRIO, 0);
        bitwiseAndWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    } else {
        bitwiseOrWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    }
}

void bms_can_data(SegmentData_t* PackData, uint8_t* bms_mod_counter, uint8_t* bms_segment_counter,
                  osMessageQueueId_t* Queue_CAN_TxHandle, Mutex_Struct_t* mutex_struct) {
    can_msg local_msg;
    SegmentData_t local_pack_data[TOTAL_MODULES];

    copyWithMutex(mutex_struct->pack_segments_key, &local_pack_data, PackData);

    // BMS CAN ID conversion based on which Module and starting Cell is given to the function
    local_msg.id = CAN_ID_BMS_INIT + ((*bms_segment_counter) * 6) + ((*bms_mod_counter + 1) / 4);
    local_msg.length = FDCAN_DLC_BYTES_8;
    // Voltage and Temperature Assignment
    for (int i = 0; i < 4; i++) {
        // Assign voltages and temperatures
        uint32_t temp_idx =
            cell_to_temp_index(*bms_mod_counter);  // Special conversion with offset num of temp
                                                   // sensors and voltage sensors
        local_msg.data[i] = clamp_u8(
            (((local_pack_data[*bms_segment_counter].cell_v_mV[*bms_mod_counter]) / (1000.0f) -
              1.8) /
             0.01),
            0.0f, 255.0f);
        local_msg.data[i + 4] =
            clamp_u8(local_pack_data[*bms_segment_counter].temp_C[temp_idx] * 4.0f, 0.0f, 255.0f);
        *bms_mod_counter += 1;
        // Check for end of module
        if (*bms_mod_counter >= (uint8_t)CELLS_PER_MOD) {
            *bms_segment_counter += 1;
            // Check for final Module
            if (*bms_segment_counter == TOTAL_MODULES) {
                *bms_segment_counter = 0;
            }
            *bms_mod_counter = 0;
            // Fill in rest of the data with zeros
            for (int j = i + 1; j < 4; j++) {
                local_msg.data[j] = clamp_u8((uint8_t)0.0f, 0.0f, 255.0f);
                local_msg.data[j + 4] = clamp_u8((uint8_t)0.0f, 0.0f, 255.0f);
            }
            break;
        }
    }

    if (osMessageQueueGetSpace(Queue_CAN_TxHandle) > 0) {
        osMessageQueuePut(Queue_CAN_TxHandle, &local_msg, CAN_TX_QUEUE_HIGH_PRIO, 0);
        bitwiseAndWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    } else {
        bitwiseOrWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    }
}

void bsm_can(bsm_obj* bsmInfo, GPIO_Info_t* gpio_data, osMessageQueueId_t* Queue_CAN_TxHandle,
             Mutex_Struct_t* mutex_struct) {
    // Send battery state machine outputs and state
    can_msg local_msg;
    bsm_obj local_bsm;
    GPIO_Info_t local_gpio_info;
    copyWithMutex(mutex_struct->bsm_key, &local_bsm, bsmInfo);
    copyWithMutex(mutex_struct->gpio_data_key, &local_gpio_info, gpio_data);

    local_msg.data[0] = (int8_t)local_bsm.state;
    local_msg.data[1] = local_bsm.pc_enable;
    local_msg.data[2] = local_bsm.ir_plus_enable;
    local_msg.data[3] = local_bsm.ir_minus_enable;
    local_msg.data[5] = local_gpio_info.ir_plus_aux;
    local_msg.data[6] = local_gpio_info.ir_minus_aux;
    if (local_bsm.state == FAULT) {
        local_msg.data[4] = (uint8_t)1;
    } else {
        local_msg.data[4] = 0;
    }
    local_msg.id = CAN_ID_BSM;
    local_msg.length = FDCAN_DLC_BYTES_7;

    if (osMessageQueueGetSpace(Queue_CAN_TxHandle) > 0) {
        osMessageQueuePut(Queue_CAN_TxHandle, &local_msg, CAN_TX_QUEUE_HIGH_PRIO, 0);
        bitwiseAndWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    } else {
        bitwiseOrWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    }
}

void bms_can_ids(SegmentData_t* PackData, TotalPack_t* pack, osMessageQueueId_t* Queue_CAN_TxHandle,
                 Mutex_Struct_t* mutex_struct) {
    can_msg local_msg;
    SegmentData_t local_pack_data[TOTAL_MODULES];
    TotalPack_t local_pack;
    copyWithMutex(mutex_struct->pack_segments_key, &local_pack_data, PackData);
    copyWithMutex(mutex_struct->total_pack_key, &local_pack, pack);

    // Send individual BMS board IDs
    for (int cic = 0; cic < TOTAL_MODULES; cic++) {
        local_msg.data[cic] = (local_pack_data[cic].id[1] << 4U) | (local_pack_data[cic].id[0]);
    }

    local_msg.data[6] = clamp_u8(local_pack.temp * 4.0f, 0.0f, 255.0f);
    local_msg.data[7] = clamp_u8(((local_pack.avg_voltage - 1.8) / 0.01), 0.0f, 255.0f);

    local_msg.id = CAN_ID_BMS_IDS;
    local_msg.length = FDCAN_DLC_BYTES_8;

    if (osMessageQueueGetSpace(Queue_CAN_TxHandle) > 0) {
        osMessageQueuePut(Queue_CAN_TxHandle, &local_msg, CAN_TX_QUEUE_LOW_PRIO, 0);
        bitwiseAndWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    } else {
        bitwiseOrWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    }
}

void soc_can_stats(TotalPack_t* pack, osMessageQueueId_t* Queue_CAN_TxHandle,
                   Mutex_Struct_t* mutex_struct) {
    can_msg local_msg;
    TotalPack_t local_pack;

    copyWithMutex(mutex_struct->total_pack_key, &local_pack, pack);

    // Overall Pack SOC Information

    float clamped_cap = fmaxf(fminf(10.4 * 144, local_pack->capacity), 0);
    clamped_cap = clamped_cap / (10.4 * 144);
    local_msg.data[0] = (uint8_t)((uint16_t)(local_pack->soc * 65535.0f) & 0xFF);
    local_msg.data[1] = (uint8_t)(((uint16_t)(local_pack->soc * 65535.0f)) >> 8U);
    local_msg.data[2] = (uint8_t)((uint16_t)(clamped_cap * 65535.0f) & 0xFF);
    local_msg.data[3] = (uint8_t)(((uint16_t)(clamped_cap * 65535.0f)) >> 8U);
    local_msg.data[4] = (uint8_t)((uint16_t)(local_pack->uncertainty * 65535.0f) & 0xFF);
    local_msg.data[5] = (uint8_t)(((uint16_t)(local_pack->uncertainty * 65535.0f)) >> 8U);

    local_msg.id = CAN_ID_SOC_STATS;
    local_msg.length = FDCAN_DLC_BYTES_6;

    if (osMessageQueueGetSpace(Queue_CAN_TxHandle) > 0) {
        osMessageQueuePut(Queue_CAN_TxHandle, &local_msg, CAN_TX_QUEUE_LOW_PRIO, 0);
        bitwiseAndWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    } else {
        bitwiseOrWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    }
}

void soc_can_data(SOC_Estimate soc[][CELLS_PER_MOD], uint8_t* soc_mod_counter,
                  uint8_t* soc_segment_counter, osMessageQueueId_t* Queue_CAN_TxHandle,
                  Mutex_Struct_t* mutex_struct) {
    // SOC CAN ID conversion based on which Module and starting Cell is given to the function
    can_msg local_msg;
    SOC_Estimate local_soc[TOTAL_MODULES][TOTAL_CELLS];
    copyWithMutex(mutex_struct->soc_estimate_key, &local_soc, soc);

    local_msg.id = CAN_ID_SOC_INIT + ((*soc_segment_counter) * 3) + ((*soc_mod_counter + 1) / 8);
    local_msg.length = FDCAN_DLC_BYTES_8;
    for (int i = 0; i < 8; i++) {
        // SOC Assignment
        local_msg.data[i] =
            (uint8_t)(local_soc[*soc_segment_counter][*soc_mod_counter].soc * 255.0f);
        *soc_mod_counter += 1;
        // Check for end of Module
        if ((*soc_mod_counter + 1) % ((uint8_t)CELLS_PER_MOD + 1) == 0) {
            *soc_segment_counter += 1;
            // Check for last module
            if (*soc_segment_counter == TOTAL_MODULES) {
                *soc_segment_counter = 0;
            }
            *soc_mod_counter = 0;
            // Fill in rest of the data with zeros
            for (int j = i + 1; j < 8; j++) {
                local_msg.data[j] = clamp_u8((uint8_t)0.0f, 0.0f, 255.0f);
            }
            break;
        }
    }

    if (osMessageQueueGetSpace(Queue_CAN_TxHandle) > 0) {
        osMessageQueuePut(Queue_CAN_TxHandle, &local_msg, CAN_TX_QUEUE_LOW_PRIO, 0);
        bitwiseAndWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    } else {
        bitwiseOrWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    }
}

void error_can(osMessageQueueId_t* Queue_CAN_TxHandle, Mutex_Struct_t* mutex_struct) {
    // Send error bits
    can_msg local_msg;
    Errors local_error_info;
    copyWithMutex(mutex_struct->error_info_key, &local_error_info, &error_info);

    local_msg.data[0] = local_error_info.message_send_errors;
    local_msg.data[1] = local_error_info.message_receive_errors;
    local_msg.data[2] = local_error_info.message_init_send_errors;
    local_msg.data[3] = local_error_info.can_queue_errors;
    local_msg.data[4] = local_error_info.queue_info;
    local_msg.data[5] = 1;  // Message existing
    local_msg.id = CAN_ID_ERRORS;
    local_msg.length = FDCAN_DLC_BYTES_6;

    if (osMessageQueueGetSpace(Queue_CAN_TxHandle) > 0) {
        osMessageQueuePut(Queue_CAN_TxHandle, &local_msg, CAN_TX_QUEUE_LOW_PRIO, 0);
        bitwiseAndWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    } else {
        bitwiseOrWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    }
}

void adc_can(ADC_Inputs_t* adc_data, osMessageQueueId_t* Queue_CAN_TxHandle,
             Mutex_Struct_t* mutex_struct) {
    // Convert ADCs and send
    can_msg local_msg;
    ADC_Inputs_t local_adc;
    copyWithMutex(mutex_struct->adc_data_key, &local_adc, adc_data);

    uint16_t ts12 = clamp_u16(adc_data->ts_vsense / PACK_VOLTAGE_SCALE, 0, 4095);
    uint16_t bat12 = clamp_u16(adc_data->bat_vsense / PACK_VOLTAGE_SCALE, 0, 4095);
    local_msg.data[0] = ts12 & 0xFF;
    local_msg.data[1] = ((ts12 >> 8) & 0x0F) | ((bat12 << 4) & 0xF0);
    local_msg.data[2] = (bat12 >> 4) & 0xFF;
    local_msg.data[3] =
        clamp_u8((adc_data->temp_precharge - TSENSE_OFFSET) / TSENSE_SCALE, 0.0f, 255.0f);
    local_msg.data[4] =
        clamp_u8((adc_data->temp_power - TSENSE_OFFSET) / TSENSE_SCALE, 0.0f, 255.0f);
    local_msg.data[5] =
        clamp_u8((adc_data->temp_vsense - TSENSE_OFFSET) / TSENSE_SCALE, 0.0f, 255.0f);
    local_msg.data[6] =
        clamp_u8((adc_data->temp_ambient - TSENSE_OFFSET) / TSENSE_SCALE, 0.0f, 255.0f);

    local_msg.id = CAN_ID_PACK_SENSE;
    local_msg.length = FDCAN_DLC_BYTES_7;

    if (osMessageQueueGetSpace(Queue_CAN_TxHandle) > 0) {
        osMessageQueuePut(Queue_CAN_TxHandle, &local_msg, CAN_TX_QUEUE_NORM_PRIO, 0);
        bitwiseAndWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    } else {
        bitwiseOrWithMutex(mutex_struct->error_info_key, &error_info, QUEUE_CAN_SEND_ADD_FULL);
    }
}

void CAN_SendData(osMessageQueueId_t* Queue_CAN_TxHandle, FDCAN_HandleTypeDef* hfdcan1) {
    can_msg local_msg;

    if (osMessageQueueGetCount(Queue_CAN_TxHandle) > 0) {
        bitwiseAndWithMutex(mutex_struct.error_info_key, &error_info.can_queue_errors,
                            QUEUE_CAN_SEND_POP_EMPTY);
        if (osMessageQueueGet(Queue_CAN_TxHandle, &local_msg, NULL, 0) != osOK) {
            bitwiseOrWithMutex(mutex_struct.error_info_key, &error_info.can_queue_errors,
                               QUEUE_CAN_SEND_POP_ERROR);
        } else {
            bitwiseAndWithMutex(mutex_struct.error_info_key, &error_info.can_queue_errors,
                                QUEUE_CAN_SEND_POP_ERROR);
        }
    } else {
        bitwiseOrWithMutex(mutex_struct.error_info_key, &error_info.can_queue_errors,
                           QUEUE_CAN_SEND_POP_EMPTY);
    }

    uint8_t error_bit = msg_to_error_bit(can_msg.id);

    if (HAL_FDCAN_GetTxFifoFreeLevel(hfdcan1) == 0) {
        bitwiseOrWithMutex(mutex_struct.error_info_key, &error_info.message_send_errors, error_bit);
    }

    // Set up the transmit header with the proper ID and length
    FDCAN_TxHeaderTypeDef txHdr;
    txHdr.Identifier = local_msg.id;
    txHdr.IdType = FDCAN_STANDARD_ID;
    txHdr.TxFrameType = FDCAN_DATA_FRAME;
    txHdr.DataLength = local_msg.length;
    txHdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHdr.BitRateSwitch = FDCAN_BRS_OFF;
    txHdr.FDFormat = FDCAN_CLASSIC_CAN;  // match your init FrameFormat
    txHdr.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    txHdr.MessageMarker = 0;

    if (HAL_FDCAN_AddMessageToTxFifoQ(hfdcan1, &txHdr, local_msg.data) != HAL_OK) {
        bitwiseOrWithMutex(mutex_struct.error_info_key, &error_info.message_send_errors, error_bit);
    } else {
        bitwiseAndWithMutex(mutex_struct.error_info_key, &error_info, error_bit);
    }
}

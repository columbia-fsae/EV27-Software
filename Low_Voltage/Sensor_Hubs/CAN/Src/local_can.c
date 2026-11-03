#include "local_can.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "Common.h"
#include "cmsis_os2.h"
#include "global_can.h"
#include "main.h"
#include "stm32g4xx_hal_fdcan.h"

static uint8_t RxData[8];
static FDCAN_RxHeaderTypeDef RxHeader;

struct temp temp_data[16] = {
    {CAN_ID_TEMP_FL_IN1, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_FL_IN2, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_FL_IN3, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_FL_IN4, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_FR_IN1, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_FR_IN2, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_FR_IN3, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_FR_IN4, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_RL_IN1, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_RL_IN2, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_RL_IN3, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_RL_IN4, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_RR_IN1, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_RR_IN2, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_RR_IN3, {0}, FDCAN_DLC_BYTES_0, false},
    {CAN_ID_TEMP_RR_IN4, {0}, FDCAN_DLC_BYTES_0, false},
};

void can_init(FDCAN_HandleTypeDef* hfdcan) {
    // Start FDCAN
    if (HAL_FDCAN_Start(hfdcan) != HAL_OK) {
        Error_Handler();
        return;
    }

    // Activate the Notification for new data in FIFO0 for FDCAN1
    if (HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        Error_Handler();
        return;
    }
}

// Runs in the FDCAN interrupt: RTOS mutexes can't be used here (osMutexAcquire returns
// osErrorISR without locking). can_data fields are single 32-bit-or-smaller stores, which
// are atomic on Cortex-M4, so they're written directly; tasks snapshot can_data inside a
// critical section. error_info is read-modify-write, so it uses the ISR critical section.
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs,
                               Mutex_Struct_t* mutex_struct,
                               osMessageQueueId_t* Local_Temp_CAN_QueueHandle) {
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET) {
        // Retrieve Rx Messages from Rx FIFO0
        while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0)) {
            if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK) {
                break;  // Reception error
            } else {
                UBaseType_t isr_state = taskENTER_CRITICAL_FROM_ISR();
                if (rx_ok) {
                    error_info.general_errors &= ~(1 << CAN_RECEPTION_ERROR);
                } else {
                    error_info.general_errors |= (1 << CAN_RECEPTION_ERROR);
                }
                taskEXIT_CRITICAL_FROM_ISR(isr_state);

                // Switch Statement that reads CAN message ID to fill in correct information
                switch (RxHeader.Identifier) {
                    case CAN_ID_TEMP_FL_IN1 ... CAN_ID_TEMP_RR_IN4:
                        can_receive_msg msg;
                        msg.data = RxData;
                        msg.id = RxHeader.Identifier;
                        msg.data_length = RxHeader.DataLength;
                        if (osMessageQueueGetSpace(*Local_Temp_CAN_QueueHandle) > 0) {
                            osMessageQueuePut(*Local_Temp_CAN_QueueHandle, &msg,
                                              CAN_TX_QUEUE_HIGH_PRIO, 0);
                            bitwiseAndWithMutex(mutex_struct->error_info_key,
                                                &error_info.general_errors,
                                                QUEUE_CAN_RECEIVE_ADD_FULL);
                        } else {
                            bitwiseOrWithMutex(mutex_struct->error_info_key,
                                               &error_info.general_errors,
                                               QUEUE_CAN_RECEIVE_ADD_FULL);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

void Process_Temp_CAN_Receive(Mutex_Struct_t* mutex_struct,
                              osMessageQueueId_t* Local_Temp_CAN_QueueHandle,
                              struct temp (*temp_data)[16]) {
    while (osMessageQueueGetCount(Local_Temp_CAN_QueueHandle) > 0) {
        can_receive_msg curr_msg;
        if (osMessageQueueGet(Local_Temp_CAN_QueueHandle, *curr_msg, CAN_TX_QUEUE_HIGH_PRIO, 0) !=
            osOk) {
            bitwiseOrWithMutex(mutex_struct.error_info_key, &error_info.general_errors,
                               QUEUE_CAN_RECEIVE_POP_ERROR);
        } else {
            bitwiseAndWithMutex(mutex_struct.error_info_key, &error_info.general_errors,
                                QUEUE_CAN_RECEIVE_POP_ERROR);
        }
        index = curr_msg.id - CAN_ID_TEMP_FL_IN1;
        if (index < 0 || index > (CAN_ID_TEMP_RR_IN4 - CAN_ID_TEMP_FL_IN1)) {
            continue;
        }
        temp local_temp_info;
        local_temp_info.data = curr_msg.data;
        local_temp_info.flag = true;
        local_temp_info.id = curr_msg.id;
        local_temp_info.data_length = curr_msg.data_length;
        copyWithMutex(mutex_struct->temp_data_key, &((*temp_data)[i]), &local_temp_info);
    }
}

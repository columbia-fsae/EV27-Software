#include "global_can.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "Common.h"
#include "cmsis_os2.h"
#include "main.h"
#include "stm32g4xx_hal_fdcan.h"

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
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs) {
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET) {
        // Retrieve Rx Messages from Rx FIFO0
        bool rx_ok = HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK;

        UBaseType_t isr_state = taskENTER_CRITICAL_FROM_ISR();
        if (rx_ok) {
            error_info.general_errors &= ~(1 << CAN_RECEPTION_ERROR);
        } else {
            error_info.general_errors |= (1 << CAN_RECEPTION_ERROR);
        }
        taskEXIT_CRITICAL_FROM_ISR(isr_state);

        if (!rx_ok) {
            return;  // Reception Error
        }

        // Switch Statement that reads CAN message ID to fill in correct information
        switch (RxHeader.Identifier) {
            case CAN_ID_CHARGER:
                can_data.balancing_enable = (RxData[0] & 0b1);
                break;
            case CAN_ID_INVERTER_CURRENT:
                can_data.tractive_current =
                    (float)((int16_t)((((uint16_t)RxData[7]) << 8) | ((uint16_t)RxData[6]))) * 0.1f;
                break;
            case CAN_ID_INVERTER_VOLTAGE:
                can_data.dc_bus_voltage =
                    (float)((((uint16_t)RxData[1]) << 8) | ((uint16_t)RxData[0])) * 0.1f;
                break;
            case CAN_ID_ELCON_CURRENT:
                can_data.tractive_current =
                    (float)((int16_t)((((uint16_t)RxData[2]) << 8) | ((uint16_t)RxData[3]))) * 0.1f;
                break;
            default:
                break;
        }
    }
}

void temps_can(Mutex_Struct_t* mutex_struct, osMessageQueueId_t* queue_can_tx_handle,
               struct temp (*temp_data)[16]) {
    can_msg local_msg;
    temp msg;
    for (i = 0; i < 16; i++) {
        if ((*temp_data)[i].flag == true) {
            copyWithMutex(mutex_struct->temp_data_key, &msg, &((*temp_data)[i]));
            local_msg.data = msg.data;
            local_msg.id = msg.id;
            local_msg.length = msg.data_length;
            msg.flag = false;
            copyWithMutex(mutex_struct->temp_data_key, &((*temp_data)[i]), &msg);
            if (osMessageQueueGetSpace(*queue_can_tx_handle) > 0) {
                osMessageQueuePut(*queue_can_tx_handle, &local_msg, QUEUE_HIGH_PRIO, 0);
                bitwiseAndWithMutex(mutex_struct->error_info_key, &error_info.general_errors,
                                    QUEUE_CAN_SEND_ADD_FULL);
            } else {
                bitwiseOrWithMutex(mutex_struct->error_info_key, &error_info.general_errors,
                                   QUEUE_CAN_SEND_ADD_FULL);
            }
        }
    }
}

void CAN_SendData(osMessageQueueId_t* queue_can_tx_handle, FDCAN_HandleTypeDef* hfdcan1) {
    can_msg local_msg = {0};

    if (osMessageQueueGetCount(*queue_can_tx_handle) > 0) {
        bitwiseAndWithMutex(mutex_struct.error_info_key, &error_info.general_errors,
                            QUEUE_CAN_SEND_POP_EMPTY);
        if (osMessageQueueGet(*queue_can_tx_handle, &local_msg, NULL, 0) != osOK) {
            bitwiseOrWithMutex(mutex_struct.error_info_key, &error_info.general_errors,
                               QUEUE_CAN_SEND_POP_ERROR);
        } else {
            bitwiseAndWithMutex(mutex_struct.error_info_key, &error_info.general_errors,
                                QUEUE_CAN_SEND_POP_ERROR);
        }
    } else {
        bitwiseOrWithMutex(mutex_struct.error_info_key, &error_info.general_errors,
                           QUEUE_CAN_SEND_POP_EMPTY);
    }

    uint8_t error_bit = msg_to_error_bit(local_msg.id);

    if (HAL_FDCAN_GetTxFifoFreeLevel(hfdcan1) == 0 && error_bit != UNKNOWN_ERROR) {
        bitwiseOrWithMutex(mutex_struct.error_info_key, &error_info.message_send_errors, error_bit);
    }

    // Set up the transmit header with the proper ID and length
    FDCAN_TxHeaderTypeDef tx_hdr;
    tx_hdr.Identifier = local_msg.id;
    tx_hdr.IdType = FDCAN_STANDARD_ID;
    tx_hdr.TxFrameType = FDCAN_DATA_FRAME;
    tx_hdr.DataLength = local_msg.length;
    tx_hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_hdr.BitRateSwitch = FDCAN_BRS_OFF;
    tx_hdr.FDFormat = FDCAN_CLASSIC_CAN;  // match your init FrameFormat
    tx_hdr.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx_hdr.MessageMarker = 0;

    if (HAL_FDCAN_AddMessageToTxFifoQ(hfdcan1, &tx_hdr, local_msg.data) != HAL_OK &&
        error_bit != UNKNOWN_ERROR) {
        bitwiseOrWithMutex(mutex_struct.error_info_key, &error_info.message_send_errors, error_bit);
    } else if (error_bit != UNKNOWN_ERROR) {
        bitwiseAndWithMutex(mutex_struct.error_info_key, &error_info.message_send_errors,
                            error_bit);
    }
}

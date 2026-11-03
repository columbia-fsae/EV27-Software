#include "wss_driver.h"

#include "Common.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "main.h"
#include "projdefs.h"
#include "queue.h"
#include "stm32g4xx_hal_tim.h"

/* Measure Frequency */
float wss = 0;

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* htim) {
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        if (Is_First_Captured == 0) {  // if the first rising edge is not captured
            IC_Val1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);  // read the first value
            Is_First_Captured = 1;  // set the first captured as true
        }

        else {  // If the first rising edge is captured, now we will capture the second edge

            IC_Val2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);  // read second value
            if (IC_Val2 > IC_Val1) {
                Difference = IC_Val2 - IC_Val1;
            } else if (IC_Val1 > IC_Val2) {
                Difference = (0xffffffff - IC_Val1) + IC_Val2;
            }

            float refClock = TIMCLOCK / (PRESCALAR);

            float frequency = refClock / Difference;

            wss = frequency / PULSES_PER_REV * CIRCUMFERENCE;
            if (osMessageQueueGetSpace() == 0) {
                QueueHandle_t temp_WSS_Native_Queue = (QueueHandle_t)WSS_QueueHandle;
                xQueueOverwriteFromISR(temp_WSS_Native_Queue, &wss,
                                       pdFALSE);  // idk about this pd thingy
            } else {
                osMessageQueuePut(WSS_QueueHandle, &wss, QUEUE_NORM_PRIO, 0);
            }
            osEventFlagsSet(wss_write_flagHandle, 0x0001);

            __HAL_TIM_SET_COUNTER(htim, 0);  // reset the counter
            Is_First_Captured = 0;           // set it back to false
        }
    }
}

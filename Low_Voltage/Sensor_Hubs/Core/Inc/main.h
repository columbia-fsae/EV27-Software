/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Common.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ALERT_Pin               GPIO_PIN_13
#define ALERT_GPIO_Port         GPIOC
#define ALERT_EXTI_IRQn         EXTI15_10_IRQn
#define HSE_OSC_IN_Pin          GPIO_PIN_0
#define HSE_OSC_IN_GPIO_Port    GPIOF
#define HSE_OSC_OUT_Pin         GPIO_PIN_1
#define HSE_OSC_OUT_GPIO_Port   GPIOF
#define WSS_MCU_Pin             GPIO_PIN_2
#define WSS_MCU_GPIO_Port       GPIOA
#define Analog_4_Pin            GPIO_PIN_3
#define Analog_4_GPIO_Port      GPIOA
#define Analog_3_Pin            GPIO_PIN_7
#define Analog_3_GPIO_Port      GPIOA
#define Analog_2_Pin            GPIO_PIN_2
#define Analog_2_GPIO_Port      GPIOB
#define Local_CAN_RX_Pin        GPIO_PIN_12
#define Local_CAN_RX_GPIO_Port  GPIOB
#define Offboard_SCLK_Pin       GPIO_PIN_13
#define Offboard_SCLK_GPIO_Port GPIOB
#define Analog_1_Pin            GPIO_PIN_14
#define Analog_1_GPIO_Port      GPIOB
#define Offboard_MOSI_Pin       GPIO_PIN_15
#define Offboard_MOSI_GPIO_Port GPIOB
#define MON_SDA_Pin             GPIO_PIN_8
#define MON_SDA_GPIO_Port       GPIOA
#define MON_SCL_Pin             GPIO_PIN_9
#define MON_SCL_GPIO_Port       GPIOA
#define Offboard_MISO_Pin       GPIO_PIN_10
#define Offboard_MISO_GPIO_Port GPIOA
#define Global_CAN_RX_Pin       GPIO_PIN_11
#define Global_CAN_RX_GPIO_Port GPIOA
#define Global_CAN_TX_Pin       GPIO_PIN_12
#define Global_CAN_TX_GPIO_Port GPIOA
#define SWDIO_Pin               GPIO_PIN_13
#define SWDIO_GPIO_Port         GPIOA
#define SWCLK_Pin               GPIO_PIN_14
#define SWCLK_GPIO_Port         GPIOA
#define Offboard_SCL_Pin        GPIO_PIN_15
#define Offboard_SCL_GPIO_Port  GPIOA
#define TRACESWO_Pin            GPIO_PIN_3
#define TRACESWO_GPIO_Port      GPIOB
#define Local_CAN_TX_Pin        GPIO_PIN_6
#define Local_CAN_TX_GPIO_Port  GPIOB
#define Offboard_SDA_Pin        GPIO_PIN_7
#define Offboard_SDA_GPIO_Port  GPIOB
#define Indicator_LED_Pin       GPIO_PIN_9
#define Indicator_LED_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define GLOBAL_CAN_SEND_TIME 10
#define SENSOR_MSG_1_TIME    10
#define SENSOR_MSG_2_TIME    10
#define TEMPS_MSG_TIME       10
#define BOARD_DIAG_MSG_TIME  10
#define ADC1_PROCESS_TIME    10
#define ADC2_PROCESS_TIME    10
#define WSS_PROCESS_TIME     10

typedef struct MutexHolder {
    osMutexId_t error_info_key;
    osMutexId_t temp_data_key;
    osMutexID_t adc1_key;
    osMutexID_t adc2_key;
    osMutexID_t board_mon_key;
    // No key for can_data: it's written from the FDCAN RX interrupt, where mutexes can't be
    // used. Readers snapshot it inside taskENTER_CRITICAL()/taskEXIT_CRITICAL() instead.
} Mutex_Struct_t;

typedef struct SemaphoreHolder {
    osSemaphoreId_t sem_MON_I2C;
    osSemaphoreId_t sem_WSS;
    osSemaphoreId_t sem_ADC1;
} Semaphore_Struct_t;

extern Mutex_Struct_t mutex_struct;
extern Semaphore_Struct_t semaphore_struct;
extern osMessageQueueId_t WSS_QueueHandle;
extern osEventFlagsId_t wss_write_flagHandle;
extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern mon_i2c_data board_mon_data;
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

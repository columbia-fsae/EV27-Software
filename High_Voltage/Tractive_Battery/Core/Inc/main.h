/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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



/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define COUNTOF(__BUFFER__)   (sizeof(__BUFFER__) / sizeof(*(__BUFFER__)))
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define B1_EXTI_IRQn EXTI15_10_IRQn
#define RCC_OSC32_IN_Pin GPIO_PIN_14
#define RCC_OSC32_IN_GPIO_Port GPIOC
#define RCC_OSC32_OUT_Pin GPIO_PIN_15
#define RCC_OSC32_OUT_GPIO_Port GPIOC
#define RCC_OSC_IN_Pin GPIO_PIN_0
#define RCC_OSC_IN_GPIO_Port GPIOF
#define RCC_OSC_OUT_Pin GPIO_PIN_1
#define RCC_OSC_OUT_GPIO_Port GPIOF
#define Temp_Precharge_Pin GPIO_PIN_0
#define Temp_Precharge_GPIO_Port GPIOC
#define Temp_Power_Pin GPIO_PIN_1
#define Temp_Power_GPIO_Port GPIOC
#define IR_Plus_Pin GPIO_PIN_2
#define IR_Plus_GPIO_Port GPIOC
#define IR_Minus_Pin GPIO_PIN_3
#define IR_Minus_GPIO_Port GPIOC
#define TS_VSense_Pin GPIO_PIN_0
#define TS_VSense_GPIO_Port GPIOA
#define Bat_VSense_Pin GPIO_PIN_1
#define Bat_VSense_GPIO_Port GPIOA
#define LPUART1_TX_Pin GPIO_PIN_2
#define LPUART1_TX_GPIO_Port GPIOA
#define LPUART1_RX_Pin GPIO_PIN_3
#define LPUART1_RX_GPIO_Port GPIOA
#define Temp_Ambient_Pin GPIO_PIN_4
#define Temp_Ambient_GPIO_Port GPIOA
#define SDC_Pin GPIO_PIN_5
#define SDC_GPIO_Port GPIOC
#define Temp_VSense_Pin GPIO_PIN_0
#define Temp_VSense_GPIO_Port GPIOB
#define IR_Plus_EN_Pin GPIO_PIN_2
#define IR_Plus_EN_GPIO_Port GPIOB
#define Precharge_EN_Pin GPIO_PIN_12
#define Precharge_EN_GPIO_Port GPIOB
#define IR_Minus_EN_Pin GPIO_PIN_15
#define IR_Minus_EN_GPIO_Port GPIOB
#define Slow_CAN_Pin GPIO_PIN_6
#define Slow_CAN_GPIO_Port GPIOC
#define BMS_OK_Pin GPIO_PIN_9
#define BMS_OK_GPIO_Port GPIOC
#define T_SWDIO_Pin GPIO_PIN_13
#define T_SWDIO_GPIO_Port GPIOA
#define T_SWCLK_Pin GPIO_PIN_14
#define T_SWCLK_GPIO_Port GPIOA
#define MCU_MHS_Pin GPIO_PIN_15
#define MCU_MHS_GPIO_Port GPIOA
#define MCU_MLS_Pin GPIO_PIN_7
#define MCU_MLS_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
//CAN DEFINITIONS for 1 MBPS (Car) for 250 KBPS (Charger)
#define CAN_CAR_Nominal_Prescaler 1
#define CAN_CAR_Nominal_TSeg_1 136
#define CAN_CAR_Nominal_TSeg_2 33
#define CAN_CAR_Nominal_SJW 33
#define CAN_CAR_Data_Prescaler 17
#define CAN_CAR_Data_SJW 3
#define CAN_CAR_Data_TSeg_1 6
#define CAN_CAR_Data_TSeg_2 3

#define CAN_Charger_Nominal_Prescaler 5
#define CAN_Charger_Nominal_TSeg_1 108
#define CAN_Charger_Nominal_TSeg_2 27
#define CAN_Charger_Nominal_SJW 27
#define CAN_Charger_Data_Prescaler 17
#define CAN_Charger_Data_SJW 16
#define CAN_Charger_Data_TSeg_1 23
#define CAN_Charger_Data_TSeg_2 16

void reset_counter_init(void);

//Reset counter definitions for software reset
extern uint32_t reset_counter;
extern uint32_t reset_counter_magic;

//Memory access reset counter upon reset of the system
#define RESET_COUNTER_MAGIC 0xCAFEBABE

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern DMA_HandleTypeDef hdma_adc1;
extern DMA_HandleTypeDef hdma_adc2;
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

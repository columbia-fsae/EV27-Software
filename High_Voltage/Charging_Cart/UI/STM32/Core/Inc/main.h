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
#include "stm32f0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

extern CAN_HandleTypeDef hcan;
extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define DS_CS_Pin GPIO_PIN_0
#define DS_CS_GPIO_Port GPIOF
#define DS_RST_Pin GPIO_PIN_1
#define DS_RST_GPIO_Port GPIOF
#define SD_CS_Pin GPIO_PIN_1
#define SD_CS_GPIO_Port GPIOA
#define J1772_DET_Pin GPIO_PIN_2
#define J1772_DET_GPIO_Port GPIOA
#define SD_CD_Pin GPIO_PIN_3
#define SD_CD_GPIO_Port GPIOA
#define CAN_SLEEP_Pin GPIO_PIN_4
#define CAN_SLEEP_GPIO_Port GPIOA
#define FAN1_PWM_Pin GPIO_PIN_5
#define FAN1_PWM_GPIO_Port GPIOA
#define FAN2_PWM_Pin GPIO_PIN_6
#define FAN2_PWM_GPIO_Port GPIOA
#define DSCHRG_OFF_Pin GPIO_PIN_7
#define DSCHRG_OFF_GPIO_Port GPIOA
#define DK_BKLT_Pin GPIO_PIN_0
#define DK_BKLT_GPIO_Port GPIOB
#define RE_A_Pin GPIO_PIN_1
#define RE_A_GPIO_Port GPIOB
#define RE_A_EXTI_IRQn EXTI0_1_IRQn
#define DS_A0_Pin GPIO_PIN_8
#define DS_A0_GPIO_Port GPIOA
#define SWDIO_Pin GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SWCLK_Pin GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA
#define RE_B_Pin GPIO_PIN_6
#define RE_B_GPIO_Port GPIOB
#define RE_B_EXTI_IRQn EXTI4_15_IRQn
#define RE_S_Pin GPIO_PIN_7
#define RE_S_GPIO_Port GPIOB
#define RE_S_EXTI_IRQn EXTI4_15_IRQn

/* USER CODE BEGIN Private defines */
void reset_counter_init(void);

#define RESET_COUNTER_MAGIC 0xCAFEBABE

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

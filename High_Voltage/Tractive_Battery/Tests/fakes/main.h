/**
 * @file main.h
 * @brief Host-side stand-in for Core/Inc/main.h.
 *
 * The CubeMX main.h pulls in the whole HAL and declares Error_Handler. Here
 * Error_Handler is an FFF fake (see hal_fakes.h), so a test can check that an
 * error path was taken instead of the program hanging in an infinite loop.
 *
 * The pin names are copied from the "Private defines" block of
 * Core/Inc/main.h. Keep them in sync when the .ioc pinout changes.
 */
#ifndef MAIN_H
#define MAIN_H

#include "stm32g4xx_hal.h"

// clang-format off
#define TS_VSense_Pin            GPIO_PIN_0
#define TS_VSense_GPIO_Port      GPIOA
#define Bat_VSense_Pin           GPIO_PIN_1
#define Bat_VSense_GPIO_Port     GPIOA
#define Temp_Ambient_Pin         GPIO_PIN_4
#define Temp_Ambient_GPIO_Port   GPIOA
#define MCU_MHS_Pin              GPIO_PIN_15
#define MCU_MHS_GPIO_Port        GPIOA
#define Temp_VSense_Pin          GPIO_PIN_0
#define Temp_VSense_GPIO_Port    GPIOB
#define IR_Plus_EN_Pin           GPIO_PIN_2
#define IR_Plus_EN_GPIO_Port     GPIOB
#define MCU_MLS_Pin              GPIO_PIN_7
#define MCU_MLS_GPIO_Port        GPIOB
#define Precharge_EN_Pin         GPIO_PIN_12
#define Precharge_EN_GPIO_Port   GPIOB
#define IR_Minus_EN_Pin          GPIO_PIN_15
#define IR_Minus_EN_GPIO_Port    GPIOB
#define Temp_Precharge_Pin       GPIO_PIN_0
#define Temp_Precharge_GPIO_Port GPIOC
#define Temp_Power_Pin           GPIO_PIN_1
#define Temp_Power_GPIO_Port     GPIOC
#define IR_Plus_Pin              GPIO_PIN_2
#define IR_Plus_GPIO_Port        GPIOC
#define IR_Minus_Pin             GPIO_PIN_3
#define IR_Minus_GPIO_Port       GPIOC
#define SDC_Pin                  GPIO_PIN_5
#define SDC_GPIO_Port            GPIOC
#define Slow_CAN_Pin             GPIO_PIN_6
#define Slow_CAN_GPIO_Port       GPIOC
#define BMS_OK_Pin               GPIO_PIN_9
#define BMS_OK_GPIO_Port         GPIOC
// clang-format on

#endif

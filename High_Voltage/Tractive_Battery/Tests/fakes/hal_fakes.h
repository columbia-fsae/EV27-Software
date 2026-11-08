/**
 * @file hal_fakes.h
 * @brief FFF fakes for every HAL/CMSIS function the firmware under test calls.
 *
 * To fake a new function, add ONE line to HAL_FAKE_LIST:
 *
 *   VALUE(<return type>, <name>, <arg types...>)   for functions that return
 *   VOID(<name>, <arg types...>)                    for void functions
 *
 * That line declares the fake here, defines it in hal_fakes.c and adds it to
 * hal_fakes_reset(). In a test you then get <name>_fake with call_count,
 * argN_val, return_val, custom_fake and so on (see vendor/fff/fff.h).
 */
#ifndef HAL_FAKES_H
#define HAL_FAKES_H

#include <stdint.h>

#include "fff.h"
#include "stm32g4xx_hal.h"

// clang-format off
#define HAL_FAKE_LIST(VALUE, VOID)                                                           \
    /* main.h */                                                                             \
    VOID(Error_Handler)                                                                      \
    /* CMSIS interrupt masking (see support/isr_sim.h) */                                    \
    VOID(__disable_irq)                                                                      \
    VOID(__enable_irq)                                                                       \
    VALUE(uint32_t, __get_PRIMASK)                                                           \
    VOID(__set_PRIMASK, uint32_t)                                                            \
    /* Core */                                                                               \
    VALUE(uint32_t, HAL_GetTick)                                                             \
    VOID(HAL_Delay, uint32_t)                                                                \
    /* ADC / DMA */                                                                          \
    VALUE(HAL_StatusTypeDef, HAL_ADCEx_Calibration_Start, ADC_HandleTypeDef*, uint32_t)      \
    VALUE(HAL_StatusTypeDef, HAL_ADC_Start_DMA, ADC_HandleTypeDef*, uint32_t*, uint32_t)     \
    VALUE(HAL_StatusTypeDef, HAL_DMA_Start, DMA_HandleTypeDef*, uint32_t, uint32_t, uint32_t) \
    /* GPIO */                                                                               \
    VALUE(GPIO_PinState, HAL_GPIO_ReadPin, GPIO_TypeDef const*, uint16_t)                    \
    VOID(HAL_GPIO_WritePin, GPIO_TypeDef*, uint16_t, GPIO_PinState)                          \
    /* FDCAN */                                                                              \
    VALUE(HAL_StatusTypeDef, HAL_FDCAN_Init, FDCAN_HandleTypeDef*)                           \
    VALUE(HAL_StatusTypeDef, HAL_FDCAN_Start, FDCAN_HandleTypeDef*)                          \
    VALUE(HAL_StatusTypeDef, HAL_FDCAN_ActivateNotification, FDCAN_HandleTypeDef*, uint32_t, \
          uint32_t)                                                                          \
    VALUE(HAL_StatusTypeDef, HAL_FDCAN_GetRxMessage, FDCAN_HandleTypeDef*, uint32_t,         \
          FDCAN_RxHeaderTypeDef*, uint8_t*)                                                  \
    VALUE(uint32_t, HAL_FDCAN_GetTxFifoFreeLevel, FDCAN_HandleTypeDef const*)                \
    VALUE(HAL_StatusTypeDef, HAL_FDCAN_AddMessageToTxFifoQ, FDCAN_HandleTypeDef*,            \
          FDCAN_TxHeaderTypeDef const*, uint8_t const*)                                      \
    /* SPI */                                                                                \
    VALUE(HAL_StatusTypeDef, HAL_SPI_Transmit, SPI_HandleTypeDef*, uint8_t const*, uint16_t, \
          uint32_t)                                                                          \
    VALUE(HAL_StatusTypeDef, HAL_SPI_Receive, SPI_HandleTypeDef*, uint8_t*, uint16_t,        \
          uint32_t)                                                                          \
    /* Flash */                                                                              \
    VALUE(HAL_StatusTypeDef, HAL_FLASH_Unlock)                                               \
    VALUE(HAL_StatusTypeDef, HAL_FLASH_Lock)                                                 \
    VALUE(HAL_StatusTypeDef, HAL_FLASHEx_Erase, FLASH_EraseInitTypeDef*, uint32_t*)          \
    VALUE(HAL_StatusTypeDef, HAL_FLASH_Program, uint32_t, uint32_t, uint64_t)
// clang-format on

HAL_FAKE_LIST(DECLARE_FAKE_VALUE_FUNC, DECLARE_FAKE_VOID_FUNC)

// Reset every fake's call history, return values and custom_fake.
// Call it from setUp() in every test file.
void hal_fakes_reset(void);

#endif

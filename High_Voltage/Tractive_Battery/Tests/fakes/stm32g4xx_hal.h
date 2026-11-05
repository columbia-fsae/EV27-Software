/**
 * @file stm32g4xx_hal.h
 * @brief Host-side stand-in for the STM32G4 HAL.
 *
 * Firmware sources include "stm32g4xx_hal.h" (usually through main.h). In the
 * test build this file is found first, so the real HAL and CMSIS headers are
 * never compiled. It holds only the types, register bits and constants the
 * firmware under test touches. Add to it when a new test needs more.
 *
 * Register structs only list the fields the firmware uses. Field order does
 * not match the silicon, which is fine because nothing here is memory-mapped.
 *
 * HAL functions are not declared here. They are FFF fakes, declared in
 * hal_fakes.h (included at the bottom) so each test can control them.
 */
#ifndef STM32G4XX_HAL_H
#define STM32G4XX_HAL_H

#include <stdint.h>

// ---------------------------------------------------------------- Common

typedef enum {
    HAL_OK = 0x00U,
    HAL_ERROR = 0x01U,
    HAL_BUSY = 0x02U,
    HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

#define HAL_MAX_DELAY 0xFFFFFFFFU

// ---------------------------------------------------------------- DMA

typedef struct {
    void* Instance;
} DMA_HandleTypeDef;

// ---------------------------------------------------------------- ADC

typedef struct {
    volatile uint32_t ISR;
    volatile uint32_t IER;
    volatile uint32_t CR;
    volatile uint32_t CFGR;
    volatile uint32_t DR;
} ADC_TypeDef;

typedef struct {
    ADC_TypeDef* Instance;
    DMA_HandleTypeDef* DMA_Handle;
    volatile uint32_t State;
} ADC_HandleTypeDef;

// Weak callback in the real HAL, overridden by the firmware
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc);

#define ADC_CR_ADEN            (1UL << 0)
#define ADC_CR_ADSTART         (1UL << 2)
#define ADC_ISR_ADRDY          (1UL << 0)
#define ADC_CFGR_DMAEN         (1UL << 0)
#define ADC_CFGR_DMACFG        (1UL << 1)
#define ADC_SINGLE_ENDED       0x0000007FUL
#define HAL_ADC_STATE_REG_BUSY 0x00000100UL

// ---------------------------------------------------------------- GPIO

typedef struct {
    volatile uint32_t IDR;
    volatile uint32_t ODR;
} GPIO_TypeDef;

typedef enum { GPIO_PIN_RESET = 0U, GPIO_PIN_SET } GPIO_PinState;

#define GPIO_PIN_0  ((uint16_t)0x0001)
#define GPIO_PIN_1  ((uint16_t)0x0002)
#define GPIO_PIN_2  ((uint16_t)0x0004)
#define GPIO_PIN_3  ((uint16_t)0x0008)
#define GPIO_PIN_4  ((uint16_t)0x0010)
#define GPIO_PIN_5  ((uint16_t)0x0020)
#define GPIO_PIN_6  ((uint16_t)0x0040)
#define GPIO_PIN_7  ((uint16_t)0x0080)
#define GPIO_PIN_8  ((uint16_t)0x0100)
#define GPIO_PIN_9  ((uint16_t)0x0200)
#define GPIO_PIN_10 ((uint16_t)0x0400)
#define GPIO_PIN_11 ((uint16_t)0x0800)
#define GPIO_PIN_12 ((uint16_t)0x1000)
#define GPIO_PIN_13 ((uint16_t)0x2000)
#define GPIO_PIN_14 ((uint16_t)0x4000)
#define GPIO_PIN_15 ((uint16_t)0x8000)

// ---------------------------------------------------------------- Opaque handles
// Only passed around by pointer in the code under test so far. Give them real
// fields when a test needs to look inside one.

typedef struct {
    void* Instance;
} FDCAN_HandleTypeDef;

typedef struct {
    void* Instance;
} SPI_HandleTypeDef;

typedef struct {
    void* Instance;
} UART_HandleTypeDef;

typedef struct {
    void* Instance;
} I2C_HandleTypeDef;

#include "hal_fakes.h"

#endif

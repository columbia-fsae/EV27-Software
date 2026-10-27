#include "adc_driver.h"

#include <stdint.h>

#include "cmsis_os2.h"
#include "main.h"
#include "stm32g473xx.h"
#include "stm32g4xx_hal_adc.h"
#include "stm32g4xx_hal_tim.h"

uint16_t rawADC1Values[ADC1_NUM_CONVERSIONS];
uint16_t rawADC2Values[ADC2_NUM_CONVERSIONS];

void adc_init(ADC_HandleTypeDef* hadc1, ADC_HandleTypeDef* hadc2, TIM_HandleTypeDef* tim1,
              TIM_HandleTypeDef* tim2) {
    if (HAL_ADCEx_Calibration_Start(hadc1, ADC_SINGLE_ENDED) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_ADCEx_Calibration_Start(hadc2, ADC_SINGLE_ENDED) != HAL_OK) {
        Error_Handler();
    }

    // ADC1 Start DMA Conversion
    HAL_ADC_Start_DMA(hadc1, (uint32_t*)rawADC1Values, ADC1_NUM_CONVERSIONS);
    HAL_ADC_Start_DMA(hadc2, (uint32_t*)rawADC2Values, ADC2_NUM_CONVERSIONS);

    HAL_TIM_Base_Start_IT(tim1);
    HAL_TIM_Base_Start_IT(tim2);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        osSemaphoreRelease(semaphore_struct.sem_ADC1);
    } else {
        osSemaphoreRelease(osSemaphoreId_t semaphore_id)
    }
}

void convert_adc1_to_real(adc1_info* local_adc1) {
    local_adc1->lin_pot = LINPOT_OFFSET + rawADC1Values[0] * LINPOT_SCALE;
    local_adc1->mon_5v = rawADC1Values[2];
    local_adc1->adc_2 = ADC2_OFFSET + rawADC1Values[1] * ADC2_SCALE;
}

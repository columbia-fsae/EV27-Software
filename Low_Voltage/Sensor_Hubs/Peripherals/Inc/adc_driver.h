#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include "Common.h"
#include "main.h"

// ADC DMA Defines
#define ADC1_NUM_CONVERSIONS 3
#define ADC2_NUM_CONVERSIONS 2

#define LINPOT_OFFSET 1
#define LINPOT_SCALE  1
#define ADC2_OFFSET   1
#define ADC2_SCALE    1
#define MON_5V_SCALE  2

extern uint16_t rawADC1Values[ADC1_NUM_CONVERSIONS];
extern uint16_t rawADC2Values[ADC2_NUM_CONVERSIONS];

void convert_adc1_to_real(adc1_info* global_adc1, Mutex_Struct_t* mutex_struct,
                          Semaphore_Struct_t* semaphore_struct);
void convert_adc2_to_real(adc2_info* global_adc2, Mutex_Struct_t* mutex_struct,
                          Semaphore_Struct_t* semaphore_struct);
void adc_init(ADC_HandleTypeDef* hadc1, ADC_HandleTypeDef* hadc2, TIM_HandleTypeDef* tim1,
              TIM_HandleTypeDef* tim2);

#endif

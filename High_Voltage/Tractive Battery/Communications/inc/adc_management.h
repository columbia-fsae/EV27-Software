#ifndef ADC_MANAGEMENT_H
#define ADC_MANAGEMENT_H

#include "main.h"

//ADC Inputs Structure
//Voltage and Temperature Sense
typedef struct{
	float ts_vsense;
	float bat_vsense;
	float temp_ambient;
	float temp_vsense;
	float temp_power;
	float temp_precharge;
} ADC_Inputs_t;

extern ADC_Inputs_t adc_data;

void adc_init(ADC_HandleTypeDef *hadc1, ADC_HandleTypeDef *hadc);
void adc_read(ADC_Inputs_t *adc,ADC_HandleTypeDef *hadc1, ADC_HandleTypeDef *hadc2);

#endif

#include "adc_management.h"
#include "can_management.h"

//ADC Voltage and Temperature Conversion Scalars
#define ADC_VSENSE_SCALAR 0.24216f
#define ADC_LSB_VOLTAGE (3.3f/4095.0f)
#define ADC_TSENSE_RPU 10000.0f
#define ADC_TSENSE_R25 10000.0f
#define ADC_TSENSE_VREF 3.3f
#define ADC_TSENSE_LOOKUP_SIZE 27

//Temperature ADC Sensor Lookup Data Structure
const float TSENSE_LOOKUP[ADC_TSENSE_LOOKUP_SIZE][2] = {
		// T, R(T)/R(25)
		{-10, 4.651},
		{-5, 3.663},
		{0, 2.905},
		{5, 2.319},
		{10, 1.862},
		{15, 1.505},
		{20, 1.223},
		{25, 1.0},
		{30, 0.8219},
		{35, 0.6792},
		{40, 0.5641},
		{45, 0.4708},
		{50, 0.3949},
		{55, 0.3327},
		{60, 0.2816},
		{65, 0.2393},
		{70, 0.2043},
		{75, 0.1751},
		{80, 0.1506},
		{85, 0.1301},
		{90, 0.1128},
		{95, 0.09811},
		{100, 0.08564},
		{105, 0.07501},
		{110, 0.06591},
		{115, 0.05809},
		{120, 0.05136}
};

//ADC Buffers
static volatile uint16_t rawADC1Values[5];
static volatile uint16_t rawADC2Values[1];
//ADC Storage Structure
ADC_Inputs_t adc_data = {0};

void adc_init(ADC_HandleTypeDef *hadc1, ADC_HandleTypeDef *hadc2){
	//ADC Calibration Step
	if (HAL_ADCEx_Calibration_Start(hadc1, ADC_SINGLE_ENDED) != HAL_OK) {
		Error_Handler();
	}

	if (HAL_ADCEx_Calibration_Start(hadc2, ADC_SINGLE_ENDED) != HAL_OK) {
		Error_Handler();
	}

    // ADC1 Start DMA Conversion
    HAL_ADC_Start_DMA(hadc1, (uint32_t *)rawADC1Values, 5);

    //CLAUDE WRITTEN Special ADC2 DMA Handler TODO: Short the single ADC2 Pin to ADC1
    // ADC2 HAL_ADC_Start_DMA hangs on G474 when ADC1 already running
    // due to ADRDY timing issue in ADC_Enable — start manually instead
    HAL_DMA_Start(hadc2->DMA_Handle,
                  (uint32_t)&hadc2->Instance->DR,
                  (uint32_t)rawADC2Values,
                  1);

    hadc2->Instance->CR |= ADC_CR_ADEN;
    uint32_t tickstart = HAL_GetTick();
    while (!(hadc2->Instance->ISR & ADC_ISR_ADRDY)) {
        if (HAL_GetTick() - tickstart > 200) {
            Error_Handler();
        }
    }

    hadc2->Instance->CFGR |= ADC_CFGR_DMAEN | ADC_CFGR_DMACFG;
    hadc2->Instance->CR   |= ADC_CR_ADSTART;
    hadc2->State = HAL_ADC_STATE_REG_BUSY;
}

//Temperature Conversion for ADCs
float temp_convert(uint16_t adc_value) {
	float vadc = adc_value * ADC_LSB_VOLTAGE;

	float vtop = ADC_TSENSE_VREF - vadc;
	if (vtop <= 0) {
		return TSENSE_LOOKUP[0][0];
	}

	// resistance as a ratio of the 25C value
	float rntc = (vadc/vtop) * ADC_TSENSE_RPU / ADC_TSENSE_R25;

	if (rntc > TSENSE_LOOKUP[0][1]) {
		return TSENSE_LOOKUP[0][0];
	}

	if (rntc < TSENSE_LOOKUP[ADC_TSENSE_LOOKUP_SIZE - 1][1]) {
		return TSENSE_LOOKUP[ADC_TSENSE_LOOKUP_SIZE - 1][0];
	}

	int i;
	for (i = 1; i < ADC_TSENSE_LOOKUP_SIZE; i++) {
		if (rntc > TSENSE_LOOKUP[i][1]) {
			break;
		}
	}

	//Clamping Output
	return (rntc - TSENSE_LOOKUP[i-1][1])/(TSENSE_LOOKUP[i][1] - TSENSE_LOOKUP[i-1][1]) * (TSENSE_LOOKUP[i][0] - TSENSE_LOOKUP[i-1][0]) + TSENSE_LOOKUP[i-1][0];
}

//Callback to read ADCs
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	//Voltage Sense
	adc_data.ts_vsense = rawADC1Values[0] * ADC_VSENSE_SCALAR;
	adc_data.bat_vsense = rawADC1Values[1] * ADC_VSENSE_SCALAR;

	//Temperature Sense
	adc_data.temp_precharge = temp_convert(rawADC1Values[2]);
	adc_data.temp_power = temp_convert(rawADC1Values[3]);
	adc_data.temp_vsense = temp_convert(rawADC1Values[4]);
	adc_data.temp_ambient = temp_convert(rawADC2Values[0]);
}

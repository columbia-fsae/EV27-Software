#include "gpio_management.h"

GPIO_Info_t gpio_data = {0};

#define IR_PLUS_AUX_INVERTING 1
#define IR_MINUS_AUX_INVERTING 1
#define SLOW_CAN_INVERTING 1
#define SDC_OK_INVERTING 1
#define MCU_MHS_INVERTING 1
#define MCU_MLS_INVERTING 1
#define IR_PLUS_EN_INVERTING 0
#define IR_MINUS_EN_INVERTING 0
#define PRECHARGE_EN_INVERTING 0

void GPIO_Read(GPIO_Info_t *gpio_data)
{
	gpio_data->ir_plus_aux  = IR_PLUS_AUX_INVERTING  ^ (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_2)  == GPIO_PIN_SET);
	gpio_data->ir_minus_aux = IR_MINUS_AUX_INVERTING ^ (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_3)  == GPIO_PIN_SET);
    gpio_data->slow_CAN     = SLOW_CAN_INVERTING	 ^ (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6)  == GPIO_PIN_SET);
    gpio_data->sdc_ok       = SDC_OK_INVERTING       ^ (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5)  == GPIO_PIN_SET);
    gpio_data->mcu_mhs      = MCU_MHS_INVERTING      ^ (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) == GPIO_PIN_SET);
    gpio_data->mcu_mls      = MCU_MLS_INVERTING      ^ (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7)  == GPIO_PIN_SET);
}

void GPIO_Write(bsm_obj *bsm)
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2,  (bsm->ir_plus_enable  ^ IR_PLUS_EN_INVERTING)   ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, (bsm->ir_minus_enable ^ IR_MINUS_EN_INVERTING)  ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, (bsm->pc_enable       ^ PRECHARGE_EN_INVERTING) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

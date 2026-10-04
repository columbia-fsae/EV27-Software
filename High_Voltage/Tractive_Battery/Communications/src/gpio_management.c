#include "gpio_management.h"

#include "comms_config.h"

GPIO_Info_t gpio_data = {0};

void GPIO_Read(GPIO_Info_t* gpio_data) {
    gpio_data->ir_plus_aux =
        IR_PLUS_AUX_INVERTING ^
        (HAL_GPIO_ReadPin(GPIO_IR_PLUS_AUX_PORT, GPIO_IR_PLUS_AUX_PIN) == GPIO_PIN_SET);
    gpio_data->ir_minus_aux =
        IR_MINUS_AUX_INVERTING ^
        (HAL_GPIO_ReadPin(GPIO_IR_MINUS_AUX_PORT, GPIO_IR_MINUS_AUX_PIN) == GPIO_PIN_SET);
    gpio_data->slow_CAN = SLOW_CAN_INVERTING ^
                          (HAL_GPIO_ReadPin(GPIO_SLOW_CAN_PORT, GPIO_SLOW_CAN_PIN) == GPIO_PIN_SET);
    gpio_data->sdc_ok =
        SDC_OK_INVERTING ^ (HAL_GPIO_ReadPin(GPIO_SDC_OK_PORT, GPIO_SDC_OK_PIN) == GPIO_PIN_SET);
    gpio_data->mcu_mhs =
        MCU_MHS_INVERTING ^ (HAL_GPIO_ReadPin(GPIO_MCU_MHS_PORT, GPIO_MCU_MHS_PIN) == GPIO_PIN_SET);
    gpio_data->mcu_mls =
        MCU_MLS_INVERTING ^ (HAL_GPIO_ReadPin(GPIO_MCU_MLS_PORT, GPIO_MCU_MLS_PIN) == GPIO_PIN_SET);
}

void GPIO_Write(bsm_obj* bsm) {
    HAL_GPIO_WritePin(GPIO_IR_PLUS_EN_PORT, GPIO_IR_PLUS_EN_PIN,
                      (bsm->ir_plus_enable ^ IR_PLUS_EN_INVERTING) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(
        GPIO_IR_MINUS_EN_PORT, GPIO_IR_MINUS_EN_PIN,
        (bsm->ir_minus_enable ^ IR_MINUS_EN_INVERTING) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIO_PRECHARGE_EN_PORT, GPIO_PRECHARGE_EN_PIN,
                      (bsm->pc_enable ^ PRECHARGE_EN_INVERTING) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

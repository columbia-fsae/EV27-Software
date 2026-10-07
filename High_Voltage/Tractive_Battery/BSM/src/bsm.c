#include "bsm.h"

#include <stdbool.h>

#include "adc_management.h"
#include "cmsis_os2.h"
#include "common_types.h"
#include "main.h"

/*....................................................................*/
// Set all states to FALSE
void bsm_init(bsm_obj* me) {
    me->pc_enable = false;
    me->ir_minus_enable = false;
    me->ir_plus_enable = false;
    TRAN(me, start_dis);
}

void bsm_run(bsm_obj* me, GPIO_Info_t* gpio, ADC_Inputs_t* adc, TotalPack_t* pack,
             Mutex_Struct_t* mutex_struct) {
    GPIO_Info_t local_gpio_data;
    ADC_Inputs_t local_adc_inputs;
    TotalPack_t local_pack;
    bsm_obj local_bsm;

    copyWithMutex(mutex_struct->gpio_data_key, &local_gpio_data, gpio);
    copyWithMutex(mutex_struct->adc_data_key, &local_adc_inputs, adc);
    copyWithMutex(mutex_struct->bsm_key, &local_bsm, me);
    copyWithMutex(mutex_struct->pack_segments_key, &local_pack, pack);

    switch (local_bsm.state) {
        case start_dis: {
            // State Variable Change
            local_bsm.pc_enable = false;
            local_bsm.ir_minus_enable = false;
            local_bsm.ir_plus_enable = false;

            // Check for Transition
            if (local_gpio_data.sdc_ok && local_pack.bms_ok_OUT) {
                TRAN(&local_bsm, ir_minus_close);
            }
            break;
        }

        case ir_minus_close: {
            // State Variable Change
            local_bsm.pc_enable = false;
            local_bsm.ir_minus_enable = true;
            local_bsm.ir_plus_enable = false;

            // Check for Transition
            if (!(local_gpio_data.sdc_ok && local_pack.bms_ok_OUT)) {
                TRAN(&local_bsm, start_dis);
            } else if (local_gpio_data.ir_minus_aux) {
                TRAN(&local_bsm, precharge);
            } else if (HAL_GetTick() > (local_bsm.timer + IR_FAULT_TIME_MS)) {
                TRAN(&local_bsm, FAULT);
            }
            break;
        }
        case precharge: {
            // State Variable Change
            local_bsm.pc_enable = true;
            local_bsm.ir_minus_enable = true;
            local_bsm.ir_plus_enable = false;

            // Check for Transition
            if (!(local_gpio_data.sdc_ok && local_pack.bms_ok_OUT)) {
                TRAN(&local_bsm, start_dis);
            } else if (local_adc_inputs.ts_vsense >
                           (local_adc_inputs.bat_vsense * 0.905) &&  // TODO
                       local_adc_inputs.ts_vsense < (local_adc_inputs.bat_vsense * 1.05) &&
                       local_adc_inputs.bat_vsense > PRECHARGE_BAT_MIN_V &&
                       HAL_GetTick() > (local_bsm.timer + PRECHARGE_MIN_TIME_MS)) {
                TRAN(&local_bsm, ir_plus_close);
            } else if (HAL_GetTick() > (local_bsm.timer + PRECHARGE_FAULT_TIME_MS)) {
                TRAN(&local_bsm, FAULT);
            }
            break;
        }
        case ir_plus_close: {
            // State Variable Change
            local_bsm.pc_enable = true;
            local_bsm.ir_minus_enable = true;
            local_bsm.ir_plus_enable = true;

            // Check for Transition
            if (!(local_gpio_data.sdc_ok && local_pack.bms_ok_OUT)) {
                TRAN(&local_bsm, start_dis);
            } else if (local_gpio_data.ir_plus_aux) {
                TRAN(&local_bsm, delay_post_pc);
            } else if (HAL_GetTick() > (local_bsm.timer + IR_FAULT_TIME_MS)) {
                TRAN(&local_bsm, FAULT);
            }
            break;
        }
        case delay_post_pc: {
            // State Variable Change
            local_bsm.pc_enable = false;
            local_bsm.ir_minus_enable = true;
            local_bsm.ir_plus_enable = true;

            // Check for Transition
            if (!(local_gpio_data.sdc_ok && local_pack.bms_ok_OUT)) {
                TRAN(&local_bsm, start_dis);
            } else if (HAL_GetTick() > (local_bsm.timer + PRECHARGE_POST_DELAY_MS)) {
                TRAN(&local_bsm, driving);
            } else if (!(local_gpio_data.ir_minus_aux &&
                         local_gpio_data
                             .ir_plus_aux)) {  // MAYBE (HAL_GetTick() > (me->timer+FAULT_TIME)) ||
                TRAN(&local_bsm, FAULT);
            }
            break;
        }
        case driving: {
            // State Variable Change
            // No Change
            local_bsm.pc_enable = false;
            local_bsm.ir_minus_enable = true;
            local_bsm.ir_plus_enable = true;
            // Check for Transition
            if (!(local_gpio_data.sdc_ok && local_pack.bms_ok_OUT)) {
                TRAN(&local_bsm, start_dis);
            } else if (!(local_gpio_data.ir_minus_aux && local_gpio_data.ir_plus_aux)) {
                TRAN(&local_bsm, FAULT);
            }
            break;
        }
        case FAULT: {
            // NO TRANSITION
            local_bsm.pc_enable = false;
            local_bsm.ir_minus_enable = false;
            local_bsm.ir_plus_enable = false;
            break;
        }
    }
    copyWithMutex(mutex_struct->bsm_key, &bsm, &local_bsm);
}

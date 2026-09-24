#ifndef BSM_H
#define BSM_H

#include <stdbool.h>

#include "adc_management.h"
#include "bsm_config.h"
#include "common_types.h"
#include "gpio_management.h"
#include "main.h"

// List of all states
enum bsmStates {
    start_dis = 0x0,
    ir_minus_close = 0x1,
    precharge = 0x2,
    ir_plus_close = 0x3,
    delay_post_pc = 0x4,
    driving = 0x5,
    FAULT = 0xF
};

/*.....................................................................*/
// Transition function
// Sets state and updates timer to current time
static inline void TRAN(bsm_obj* me, uint8_t Target_) {
    if (Target_ == FAULT) {
        // If entering FAULT state, pull the BMS fault low without registering it internally, see
        // BSM state for fault condition Done to discharge the pack in the event of IR driving fault
        // or precharge issue Will not register in BMS fault flags HAL_GPIO_WritePin(GPIOC,
        // GPIO_PIN_9, GPIO_PIN_RESET);
    }
    me->state = Target_;
    me->timer = HAL_GetTick();
}

void bsm_init(bsm_obj* me);
void bsm_run(bsm_obj* me, GPIO_Info_t* gpio, ADC_Inputs_t* adc);

#endif

#ifndef BSM_H
#define BSM_H

#include <stdbool.h>

#include "adc_management.h"
#include "common_types.h"
#include "gpio_management.h"
#include "main.h"

// State Time Delay and Voltage Defines

#define PRECHARGE_FAULT_TIME_MS 60000
#define PRECHARGE_MIN_TIME_MS 25000
#define PRECHARGE_POST_DELAY_MS 1000
#define IR_FAULT_TIME_MS 1000
#define PRECHARGE_BAT_MIN_V 20.0f

// Thresholds

#define PRECHARGE_LOWER 0.905F
#define PRECHARGE_UPPER 1.005F

/*.....................................................................*/
// Transition function
// Sets state and updates timer to current time
static inline void TRAN(bsm_obj* bsm, uint8_t Target_) {
    if (Target_ == FAULT) {
        // If entering FAULT state, pull the BMS fault low without registering it internally, see
        // BSM state for fault condition Done to discharge the pack in the event of IR driving fault
        // or precharge issue Will not register in BMS fault flags HAL_GPIO_WritePin(GPIOC,
        // GPIO_PIN_9, GPIO_PIN_RESET);
    }
    bsm->bsm_state = Target_;
    bsm->timer = HAL_GetTick();
}

void bsm_init(bsm_obj* bsm);
void bsm_run(bsm_obj* bsm, GPIO_Info_t* gpio, ADC_Inputs_t* adc, TotalPack_t* pack,
             Mutex_Struct_t* mutex_struct);

#endif

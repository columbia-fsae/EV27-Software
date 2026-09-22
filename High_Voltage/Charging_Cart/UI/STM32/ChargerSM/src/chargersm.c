#include "chargersm.h"

#include "can.h"
#include "interfaceTemplate.h"
#include "stm32f0xx_hal.h"

/*....................................................................*/

static const uint8_t _ready_options[] = {3, STAY, MANUAL, AUTOMATIC};
static const uint8_t _man_charge_options[] = {3, STAY, GO_TO_READY, BALANCE_MANUAL, WAIT_MANUAL};
static const uint8_t _man_wait_options[] = {4, STAY, GO_TO_READY, BALANCE_MANUAL, CHARGE_MANUAL};
static const uint8_t _man_balance_options[] = {3, STAY, GO_TO_READY, CHARGE_MANUAL, WAIT_MANUAL};
static const uint8_t _auto_options[] = {2, STAY, GO_TO_READY};

// maybe change this to function to align with how the display gets status messages, but this is
// easier for now
const char* statusChangeMessages[] = {
    STATUS_CHANGE_MSG_STAY,          STATUS_CHANGE_MSG_GO_TO_READY,
    STATUS_CHANGE_MSG_MANUAL,        STATUS_CHANGE_MSG_AUTOMATIC,
    STATUS_CHANGE_MSG_CHARGE_MANUAL, STATUS_CHANGE_MSG_BALANCE_MANUAL,
    STATUS_CHANGE_MSG_WAIT_MANUAL,   STATUS_CHANGE_MSG_NO_OPTIONS};

const uint8_t* get_UI_options(chargersm_obj* chargersm) {
    switch (chargersm->chargerState) {
        case start:
            return NULL;
        case ready:
            return _ready_options;
        case manual:
            switch (chargersm->chargingState) {
                case charge:
                    return _man_charge_options;
                case wait:
                    return _man_wait_options;
                case balance:
                    return _man_balance_options;
            }
            // should never get here
            return NULL;
        case automatic:
            return _auto_options;
        case FAULT:
            return NULL;
    }
    // should never get here
    return NULL;
}

static const char* _start_msg = STATUS_MSG_START;
static const char* _ready_msg = STATUS_MSG_READY;
static const char* _man_balance_msg = STATUS_MSG_MANUAL_BALANCE;
static const char* _man_charge_msg = STATUS_MSG_MANUAL_CHARGE;
static const char* _man_wait_msg = STATUS_MSG_MANUAL_WAIT;
static const char* _auto_balance_msg = STATUS_MSG_AUTO_BALANCE;
static const char* _auto_charge_msg = STATUS_MSG_AUTO_CHARGE;
static const char* _auto_wait_msg = STATUS_MSG_AUTO_WAIT;
static const char* _fault_msg = NULL;  // for fault message we will do a custom one
static const char* BAD_MSG = STATUS_MSG_STM32_ERROR;

const char* get_UI_current_status(chargersm_obj* chargersm) {
    switch (chargersm->chargerState) {
        case start:
            return _start_msg;
        case ready:
            return _ready_msg;
        case manual:
            switch (chargersm->chargingState) {
                case charge:
                    return _man_charge_msg;
                case wait:
                    return _man_wait_msg;
                case balance:
                    return _man_balance_msg;
            }
            // should never get here
            return BAD_MSG;
        case automatic:
            switch (chargersm->chargingState) {
                case charge:
                    return _auto_charge_msg;
                case wait:
                    return _auto_wait_msg;
                case balance:
                    return _auto_balance_msg;
            }
            // should never get here
            return BAD_MSG;
        case FAULT:
            return _fault_msg;
    }
    // should never get here
    return BAD_MSG;
}

void chargersm_init(chargersm_obj* me) {
    me->balancing = false;
    me->balancing_flag = true;
    me->charging = false;
    TRAN(me, chargerSm, ready);
    TRAN(me, charging, charge);
}

// to ensure fault cases are consistent
static inline bool fault_case(chargersm_obj* me, volatile CANInfo* can,
                              GPIO_PinState sdc_pin_state) {
#ifdef TRAINING_WHEELS_MODE
    me->error_flags = 0;
    return false;
#endif

    me->error_flags = 0;

    me->error_flags |= can->elconStatus & ELCON_FAULT_MASK;  // bits 0-4: elcon faults
    me->error_flags |= (HAL_GetTick() - can->lastElconUpdateTick > ELCON_TIMEOUT)
                       << FAULT_BIT_ELCON_TIMEOUT;  // bit 5: Elcon CAN Timeout
    me->error_flags |=
        (can->bsmState != BSM_STATE_DRIVING && can->bsmState != BSM_STATE_FAULT)
        << FAULT_BIT_TBP_NOT_READY;  // bit 6: TBP Not Ready (bsmState not 5, but also not faulted)
    me->error_flags |= (can->bsmState == BSM_STATE_FAULT)
                       << FAULT_BIT_BSM_FAULT;  // bit 7: BSM Fault (bsmState is 0xF)
    me->error_flags |= (HAL_GetTick() - can->lastBSMUpdateTick > BSM_TIMEOUT)
                       << FAULT_BIT_BSM_TIMEOUT;                          // bit 8: BSM CAN Timeout
    me->error_flags |= (sdc_pin_state == GPIO_PIN_SET) << FAULT_BIT_SDC;  // bit 9: SDC
    me->error_flags |= (cell_voltage_conversion(can->maxVoltVal) > CELL_V_LIMIT)
                       << FAULT_BIT_CELL_OVERVOLTAGE;  // bit 10: Cell overvoltage

    return me->error_flags != 0;
}

void chargersm_run(chargersm_obj* me, volatile CANInfo* can, int16_t* userVars,
                   GPIO_PinState sdc_pin_state) {
#ifdef TRAINING_WHEELS_MODE
    sdc_pin_state = GPIO_PIN_RESET;
    can->bsmState = BSM_STATE_DRIVING;
    can->elconStatus = 0;
#endif

    switch (me->chargerState) {
        case start: {
            me->balancing = false;
            me->charging = false;
            if (!fault_case(me, can, sdc_pin_state)) {
                TRAN(me, chargerSm, ready);
            }
            break;
        }
        case ready: {
            me->balancing = false;
            me->charging = false;
            if (fault_case(me, can, sdc_pin_state)) {
                TRAN(me, chargerSm, FAULT);
            } else if (userVars[STATUS] == MANUAL) {
                me->timeSinceCharge = HAL_GetTick();
                TRAN(me, chargerSm, manual);
                TRAN(me, charging, charge);  // preset
            } else if (userVars[STATUS] == AUTOMATIC) {
                me->timeSinceCharge = HAL_GetTick();
                me->balancing_flag = true;
                TRAN(me, chargerSm, automatic);
            }
            break;
        }
        case manual: {
            if (fault_case(me, can, sdc_pin_state)) {
                TRAN(me, chargerSm, FAULT);
            } else if (userVars[STATUS] == GO_TO_READY) {
                TRAN(me, chargerSm, ready);
            } else {
                manual_charge(me, can, userVars);
            }
            break;
        }
        case automatic: {
            if (fault_case(me, can, sdc_pin_state)) {
                TRAN(me, chargerSm, FAULT);
            } else if (userVars[STATUS] == GO_TO_READY) {
                TRAN(me, chargerSm, ready);
            } else {
                auto_charge(me, can, userVars);
            }
            break;
        }
        case FAULT: {
            me->balancing = false;
            me->charging = false;
            me->lastCanUpdate = 0;          // force immediate CAN update
            userVars[STATUS] = NO_OPTIONS;  // override UI options to prevent user from trying to
                                            // start charging again without addressing the fault
            if (!fault_case(me, can, sdc_pin_state)) {
                TRAN(me, chargerSm, ready);
            }
            break;
        }
    }
}

void manual_charge(chargersm_obj* me, volatile CANInfo* can, int16_t* userVars) {
    switch (me->chargingState) {
        case charge: {
            me->charging = true;
            me->balancing = false;
            if (userVars[STATUS] == BALANCE_MANUAL) {
                TRAN(me, charging, balance);
            } else if (HAL_GetTick() - me->timer > CHARGE_WAIT || userVars[STATUS] == WAIT_MANUAL) {
                TRAN(me, charging, wait);
            }
            break;
        }
        case wait: {
            me->charging = false;
            me->balancing = false;
            if (userVars[STATUS] == BALANCE_MANUAL) {
                TRAN(me, charging, balance);
            } else if (HAL_GetTick() - me->timer > WAIT || userVars[STATUS] == CHARGE_MANUAL) {
                TRAN(me, charging, charge);
            }
            break;
        }
        case balance: {
            me->charging = false;
            me->balancing = true;
            if (userVars[STATUS] == CHARGE_MANUAL) {
                TRAN(me, charging, charge);
            } else if (userVars[STATUS] == WAIT_MANUAL) {
                TRAN(me, charging, wait);
            }
            break;
        }
    }
}

void auto_charge(chargersm_obj* me, volatile CANInfo* can, int16_t* userVars) {
    switch (me->chargingState) {
        case charge: {
            me->charging = true;
            me->balancing = false;
            if (HAL_GetTick() - me->timer > CHARGE_WAIT) {
                TRAN(me, charging, wait);
            }
            if ((can->packVoltage > (userVars[VOLTAGE_LIMIT] * BALANCING_THRESH)) &&
                me->balancing_flag) {
                TRAN(me, charging, balance);
            } else if (can->packVoltage >= userVars[VOLTAGE_LIMIT]) {
                TRAN(me, chargerSm, ready);
            }
            break;
        }
        case wait: {
            me->charging = false;
            if (HAL_GetTick() - me->timer > WAIT) {
                TRAN(me, charging, charge);
            }
            break;
        }
        case balance: {
            me->charging = false;
            me->balancing = true;
            me->balancing_flag = false;
            if (can->balancingDone) {
                if (can->packVoltage < (userVars[VOLTAGE_LIMIT] * BALANCING_THRESH_MIN)) {
                    me->balancing_flag = true;
                }
                TRAN(me, charging, charge);
            }
            break;
        }
    }
}

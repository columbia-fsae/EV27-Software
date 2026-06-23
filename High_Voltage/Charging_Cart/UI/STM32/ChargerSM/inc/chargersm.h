#ifndef CHARGERSM_H
#define CHARGERSM_H

#include "can.h"

#include <stdbool.h>
#include <stdint.h>

#include "stm32f0xx_hal.h"

#define CHARGE_WAIT 600000
#define WAIT 10000
#define BALANCING_THRESH 0.9f
#define BALANCING_THRESH_MIN 0.85f

#define BSM_TIMEOUT 5000
#define ELCON_TIMEOUT 5000
#define CELL_V_LIMIT 4200 //in mv

#ifdef __cplusplus
extern "C" {
#endif

enum chargerSmStates{
	start,
	ready,
	manual,
	automatic,
	FAULT
};

enum chargingStates{
	charge,
	wait,
	balance
};

enum types{
	chargerSm,
	charging
};

enum UIMessagePaths{
	STAY,
	GO_TO_READY,
	MANUAL,
	AUTOMATIC,
	CHARGE_MANUAL,
	BALANCE_MANUAL,
	WAIT_MANUAL,
	NO_OPTIONS
};

//should line up with UIMessagePaths, used to send status messages to the display
extern const char* statusChangeMessages[];

typedef struct {
	uint8_t chargerState;
	uint8_t chargingState;
	uint16_t error_flags; //bitfield for different errors, see below
	uint32_t timer;
	uint32_t timeSinceCharge;
	uint32_t lastCanUpdate;
	bool balancing;
	bool balancing_flag;
	bool charging;
} chargersm_obj;


#define NUM_FAULTS 11
//error_flags:
//bits 0-4: elcon faults (see datasheet)
//bit 5: Elcon CAN Timeout
//bit 6: TBP Not Ready (bsmState not 5)
//bit 7: BSM Fault (bsmState is 0xF, which is the fault state for the BSM)
//bit 8: BSM CAN Timeout
//bit 9: SDC
//bit 10: Cell overvoltage

const uint8_t* get_UI_options(chargersm_obj *chargersm);
const char* get_UI_current_status(chargersm_obj *chargersm);

static inline void TRAN(chargersm_obj *me, uint8_t Type_, uint8_t Target_) {
	 if (Type_ == charging) {
		me->timer = HAL_GetTick();
		me->chargingState = Target_;
	} else {
		me->chargerState  = Target_;
	}
}

//extern chargersm_obj chargersm;

void chargersm_init(chargersm_obj *me);
void chargersm_run(chargersm_obj *me, volatile CANInfo *can, int16_t* userVars, GPIO_PinState sdc_pin_state);
void manual_charge(chargersm_obj *me, volatile CANInfo *can, int16_t *userVars);
void auto_charge(chargersm_obj *me, volatile CANInfo *can, int16_t *userVars);

#ifdef __cplusplus
}
#endif

#endif

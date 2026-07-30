#include "bsm.h"
#include <stdbool.h>

/*....................................................................*/
//Set all states to FALSE
void bsm_init(bsm_obj *me){
	me->pc_enable = false;
	me->ir_minus_enable = false;
	me->ir_plus_enable = false;
	TRAN(me, start_dis);
}

void bsm_run(bsm_obj *me, GPIO_Info_t *gpio, ADC_Inputs_t *adc)
{
	switch(me->state){
		case start_dis:{
			//State Variable Change
			me->pc_enable = false;
			me->ir_minus_enable = false;
			me->ir_plus_enable = false;

			//Check for Transition
			if (gpio->sdc_ok && gpio->bms_ok_OUT){
				TRAN(me, ir_minus_close);
			}
			break;
		}

		case ir_minus_close:{
			//State Variable Change
			me->pc_enable = false;
			me->ir_minus_enable = true;
			me->ir_plus_enable = false;

			//Check for Transition
			if(!(gpio->sdc_ok && gpio->bms_ok_OUT)){
				TRAN(me, start_dis);
			} else if(gpio->ir_minus_aux){
				TRAN(me, precharge);
			} else if (HAL_GetTick() > (me->timer+IR_FAULT_TIME_MS)){
				TRAN(me, FAULT);
			}
			break;
		}
		case precharge:{
			//State Variable Change
			me->pc_enable = true;
			me->ir_minus_enable = true;
			me->ir_plus_enable = false;

			//Check for Transition
			if (!(gpio->sdc_ok && gpio->bms_ok_OUT)){
				TRAN(me, start_dis);
			} else if (adc->ts_vsense > (adc->bat_vsense*0.905) && // TODO
					   adc->ts_vsense < (adc->bat_vsense*1.05) &&
					   adc->bat_vsense > PRECHARGE_BAT_MIN_V &&
					   HAL_GetTick() > (me->timer+PRECHARGE_MIN_TIME_MS)) {
				TRAN(me, ir_plus_close);
			} else if (HAL_GetTick() > (me->timer+PRECHARGE_FAULT_TIME_MS)) {
				TRAN(me, FAULT);
			}
			break;
		}
		case ir_plus_close:{
			//State Variable Change
			me->pc_enable = true;
			me->ir_minus_enable = true;
			me->ir_plus_enable = true;

			//Check for Transition
			if (!(gpio->sdc_ok && gpio->bms_ok_OUT)){
				TRAN(me,start_dis);
			}else if(gpio->ir_plus_aux){
				TRAN(me, delay_post_pc);
			} else if (HAL_GetTick() > (me->timer+IR_FAULT_TIME_MS)){
				TRAN(me, FAULT);
			}
			break;
		}
		case delay_post_pc:{
			//State Variable Change
			me->pc_enable = false;
			me->ir_minus_enable = true;
			me->ir_plus_enable = true;

			//Check for Transition
			if(!(gpio->sdc_ok && gpio->bms_ok_OUT)){
				TRAN(me,start_dis);
			} else if(HAL_GetTick() > (me->timer+PRECHARGE_POST_DELAY_MS)){
				TRAN(me, driving);
			} else if (!(gpio->ir_minus_aux && gpio->ir_plus_aux)){ //MAYBE (HAL_GetTick() > (me->timer+FAULT_TIME)) ||
				TRAN(me, FAULT);
			}
			break;
		}
		case driving:{
			//State Variable Change
			//No Change
			me->pc_enable = false;
			me->ir_minus_enable = true;
			me->ir_plus_enable = true;
			//Check for Transition
			if(!(gpio->sdc_ok && gpio->bms_ok_OUT)){
				TRAN(me, start_dis);
			} else if (!(gpio->ir_minus_aux && gpio->ir_plus_aux)){
				TRAN(me, FAULT);
			}
			break;
		}
		case FAULT:{
			//NO TRANSITION
			me->pc_enable = false;
			me->ir_minus_enable = false;
			me->ir_plus_enable = false;
			break;
		}


	}
}

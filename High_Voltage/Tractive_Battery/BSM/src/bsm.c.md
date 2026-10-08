#include "bsm.h":
//include bsm.h file

#include <stdbool.h>:
//include stdbool.h file
--------------------------------------------------------------------------
void bsm_init(bsm_obj* me){ }:
Inputs:
1. a point variable named "me" with type "bsm_obj".

Function Calls:It calls the function "TRAN" 

Outputs: returns no value. Modifies "me", set all three enables to false.

--------------------------------------------------------------------------
void bsm_run(bsm_obj* me, GPIO_Info_t* gpio, ADC_Inputs_t* adc) {}:
Inputs: 
1. a point variable named "me" with type "bsm_obj" pointer.
2. a point variable named "gpio" with type "GPIO_Info_t" pointer.
3. a point variable named "adc" with type "ADC_Inputs_t" pointer.  

Function Calls: It calls the function "TRAN" and "HAL_GetTick".

Outputs: returns no value. Modifies "me" accoring to current state.

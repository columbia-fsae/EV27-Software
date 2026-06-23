#ifndef CPP_MAIN_H
#define CPP_MAIN_H

#include <stdint.h>

#include "can.h"
#include "chargersm.h"

#ifdef __cplusplus
extern "C" {
#endif

//enable display self-testing
#define SELF_TEST_DISPLAY

int cpp_main(void);
void CPP_HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

void DisplayError();

#ifdef __cplusplus
}
#endif

#endif /* CPP_MAIN_H */

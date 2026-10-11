#ifndef GPIO_MANAGEMENT_H
#define GPIO_MANAGEMENT_H

#include <stdbool.h>

#include "common_types.h"
#include "main.h"

void GPIO_Init(GPIO_Info_t* gpio);
void GPIO_Read(GPIO_Info_t* gpio, Mutex_Struct_t* mutex_struct);
void GPIO_Write(bsm_obj* bsm, Mutex_Struct_t* mutex_struct);

#endif

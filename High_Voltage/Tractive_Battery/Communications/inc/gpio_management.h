#ifndef GPIO_MANAGEMENT_H
#define GPIO_MANAGEMENT_H

#include <stdbool.h>
#include "main.h"
#include "common_types.h"

void GPIO_Read(GPIO_Info_t *gpio);
void GPIO_Write(bsm_obj *bsm);

#endif

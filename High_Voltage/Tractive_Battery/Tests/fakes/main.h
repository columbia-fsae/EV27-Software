/**
 * @file main.h
 * @brief Host-side stand-in for Core/Inc/main.h.
 *
 * The CubeMX main.h pulls in the whole HAL and declares Error_Handler. Here
 * Error_Handler is an FFF fake (see hal_fakes.h), so a test can check that an
 * error path was taken instead of the program hanging in an infinite loop.
 */
#ifndef MAIN_H
#define MAIN_H

#include "stm32g4xx_hal.h"

#endif

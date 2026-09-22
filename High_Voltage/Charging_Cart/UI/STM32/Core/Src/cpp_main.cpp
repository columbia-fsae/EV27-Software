/**
 ******************************************************************************
 * @file           : cpp_main.cpp
 * @brief          : Main program body for C++ code
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "cpp_main.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "UI_flow.hpp"
#include "can.h"
#include "chargersm.h"
#include "display.hpp"
#include "main.h"
#include "rotary_encoder.hpp"
#include "stm32f0xx_hal.h"
#include "ui_config.h"

// GLOBAL OBJECTS
// needs to be global so the callback can access it
RotaryEncoder encoder(RE_A_GPIO_Port, RE_A_Pin, RE_B_GPIO_Port, RE_B_Pin, RE_S_GPIO_Port, RE_S_Pin);

Display display(&hspi1, DS_CS_GPIO_Port, DS_CS_Pin, DS_A0_GPIO_Port, DS_A0_Pin, DS_RST_GPIO_Port,
                DS_RST_Pin, &htim3, TIM_CHANNEL_3);

// MAIN//

int cpp_main(void) {
    // init can
    can_init(&hcan);

    // STATE VARS
    chargersm_obj chargersm;
    chargersm_init(&chargersm);

    UserInputState userInputState =
        Edit_None;  // tracks if user is currently editing a value, and if so which one
    int16_t userVars[] = {
        // holds user editable variables [current_lim, volt_lim, status]
        UI_DEFAULT_CURRENT_LIMIT,  // 7.5A
        UI_DEFAULT_VOLTAGE_LIMIT,  // 250V
        0,                         // Status
    };

    display.init();
    display.on();

    /*
    //self-test display
    #ifdef SELF_TEST_DISPLAY
    for (int i = 0; i <= 10; i++){
        HAL_Delay(250);
        display.setAllInfo(1111*(i%10));
        display.refresh();
    }
    #endif
    */

    display.setAllInfo(0);

    const char* statusMsg = STATUS_MSG_START;
    const char* uiStatusMsg = nullptr;
    const char* prevStatusMsg = statusMsg;
    if (statusMsg) {
        display.setStatus(statusMsg);
    }

    // display initial user-config values
    for (uint8_t i = 0; i < NUM_EDIT_PARAMS; i++) {
        display.setInfo(static_cast<InfoType>(i), userVars[i]);
    }

    display.refresh();

    // ENCODER INIT
    encoder.resetBuffer();

    if (!error_can(&hcan)) {
        // Handle error
    }
    if (!battery_can(&hcan, 0, chargersm.balancing)) {
        // Handle error
    }
    chargersm.lastCanUpdate = HAL_GetTick();

    // NOT WORKING
    display.setBrightness(UI_BACKLIGHT_BRIGHTNESS);

    /* Infinite loop */
    while (1) {
        // TODO: MUST CALL error_can() and battery_can() in the main loop at some intervals
        if (HAL_GetTick() - chargersm.lastCanUpdate >= CAN_UPDATE_PERIOD_MS) {
            if (!error_can(&hcan)) {
                display.setStatus(STATUS_MSG_CAN_HAL_ERROR);
                display.refresh();
                HAL_Delay(CAN_ERROR_DISPLAY_MS);
            }
            if (!battery_can(&hcan, CAN_Info.elconCurrent, chargersm.balancing)) {
                display.setStatus(STATUS_MSG_CAN_HAL_ERROR);
                display.refresh();
                HAL_Delay(CAN_ERROR_DISPLAY_MS);
            }
            if (!charger_can(&hcan, userVars[VOLTAGE_LIMIT] * CHARGER_V_LIMIT_SCALE,
                             userVars[CURRENT_LIMIT], chargersm.charging)) {
                display.setStatus(STATUS_MSG_CAN_HAL_ERROR);
                display.refresh();
                HAL_Delay(CAN_ERROR_DISPLAY_MS);
            }

            chargersm.lastCanUpdate = HAL_GetTick();

            updateDisplayInfo(display, CAN_Info, chargersm);
        }

        chargersm_run(&chargersm, &CAN_Info, userVars,
                      HAL_GPIO_ReadPin(DSCHRG_OFF_GPIO_Port, DSCHRG_OFF_Pin));

        statusMsg = get_UI_current_status(&chargersm);
        processUI(userInputState, userVars, &chargersm, encoder, display, uiStatusMsg);

        if (uiStatusMsg) {
            statusMsg = uiStatusMsg;
        }

        // if theres a fault, display fault message instead of chargersm status
        if (statusMsg == nullptr) {
            setFaultMsg(display, &chargersm);
            prevStatusMsg = nullptr;

            // otherwise, display UI status message if it exists, or chargersm status message if UI
            // doesn't return a new one
        } else {
            if (statusMsg != prevStatusMsg) {  // only update display if status message has changed
                                               // to avoid unnecessary refreshes
                display.setStatus(statusMsg);
                prevStatusMsg = statusMsg;
            }
        }

        display.refresh();
    }
}

void CPP_HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    // Handle rotary encoder interrupts
    if (GPIO_Pin == RE_A_Pin || GPIO_Pin == RE_B_Pin) {
        encoder.rotationCallback();
    } else if (GPIO_Pin == RE_S_Pin) {
        encoder.switchCallback();
    }
}

void DisplayError() {
    display.clear();
    display.setStatus(STATUS_MSG_STM32_ERROR);
    display.refresh();
}

#ifndef UI_FLOW_HPP
#define UI_FLOW_HPP

#include <cstdint>

#include "chargersm.h"
#include "display.hpp"
#include "rotary_encoder.hpp"


enum UserInputState {
    Edit_Choose_Current_Limit,
    Edit_Choose_Voltage_Limit,
    Edit_Choose_Status,
    Edit_Current_Limit,
    Edit_Voltage_Limit,
    Edit_Status,
    Edit_None
};

#define NUM_EDIT_PARAMS (Edit_Status-Edit_Choose_Status)

bool processUI(UserInputState &userInputState, int16_t* userVars, chargersm_obj *chargersm, RotaryEncoder &encoder, Display &display, const char* &uiStatusMsg);
void setFaultMsg(Display& disp, chargersm_obj *me);
void updateDisplayInfo(Display& disp, const CANInfo& info, chargersm_obj &chargersm);

#endif /* UI_FLOW_HPP */
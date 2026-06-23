#include "UI_flow.hpp"

#include <cstdint>

#include "chargersm.h"
#include "display.hpp"
#include "interfaceTemplate.h"
#include "rotary_encoder.hpp"



//mod that is guaranteed to return a positive value
uint8_t mod_pos(int8_t value, int8_t mod) {
    return ((value % mod) + mod) % mod;
}

//returns true if the state is one where the user is currently choosing which value to edit (as opposed to actively editing a value)
bool isChooseState(UserInputState state) {
    return state == Edit_Choose_Current_Limit || state == Edit_Choose_Voltage_Limit
        || state == Edit_Choose_Status;
}

//returns true if the state is one where the user is currently editing a value (as opposed to choosing which value to edit)
bool isEditState(UserInputState state) {
    return state == Edit_Current_Limit || state == Edit_Voltage_Limit
        || state == Edit_Status;
}

//helper function to get the corresponding InfoType for a given UserInputState
InfoType infoFromState(UserInputState state) {
    return static_cast<InfoType>(mod_pos(static_cast<int8_t>(state), NUM_EDIT_PARAMS));
}

//returns the selected status path based on the current selected index and chargersm state, defaults to STAY if no options or invalid index
uint8_t selectedStatusPathFromIndex(uint16_t selectedIndex, chargersm_obj *chargersm) {
    const uint8_t *options = get_UI_options(chargersm);
    if (!options || options[0] == 0) {
        return NO_OPTIONS;
    }

    uint8_t numOptions = options[0];
    uint8_t clampedIndex = mod_pos(static_cast<int8_t>(selectedIndex - 1), numOptions) + 1; // ensure index is within bounds, +1 to account for options[0] being numOptions
    return options[clampedIndex];
}

int16_t clamp_user_var(int16_t value, InfoType type) {
    int16_t minVal = 0;
    int16_t maxVal = 0;

    switch (type) {
        case CURRENT_LIMIT:
            minVal = 0;
            maxVal = 140; // 14A
            break;
        case VOLTAGE_LIMIT:
            minVal = 150;
            maxVal = 252;
            break;
        case STATUS:
            // For status, the valid values depend on the chargersm state and are handled separately, so we can just return the value without clamping
            return value;
        default:
            return value; // for any non-user editable types, just return the value as is
    }

    if (value < minVal) {
        return minVal;
    } else if (value > maxVal) {
        return maxVal;
    } else {
        return value;
    }
}

//processes the UI via reading the encoder buffer
//takes in reference to the current userInputState and userVars, will change as needed.
//could also easily implement as returning the new userInputState if that's better (for checking, etc)
//returns true if the status message has changed, false otherwise
bool processUI(UserInputState &userInputState, int16_t* userVars, chargersm_obj *chargersm, RotaryEncoder &encoder, Display &display, const char* &uiStatusMsg) {
    static uint32_t editTimeoutTick = 0;
    static uint16_t currentEditValue = 0;
    int8_t net_enc_delta = 0;
    bool hasInput = encoder.peekBuffer();

    //if no input and we're currently editing, check for timeout to exit edit mode
    if (!hasInput) {
        if (userInputState != Edit_None && (HAL_GetTick() - editTimeoutTick) > 5000) {
            InfoType info = infoFromState(userInputState);
            display.boxInfo(info, false);
            display.invertInfo(info, false);
            userInputState = Edit_None;

            //set status to null
            uiStatusMsg = nullptr;
            return true;
        }
        return false;
    }
    
    //if we have input, process it and reset the timeout
    editTimeoutTick = HAL_GetTick();
    bool pressed = encoder.processBuffer(net_enc_delta);

    //if we're not currently editing, the only valid input is a press to enter edit mode
    if (userInputState == Edit_None) {
        if (pressed) {
            userInputState = Edit_Choose_Status;
            display.boxInfo(infoFromState(userInputState), true);
        }
        return false;
    }

    //if we're choosing which value to edit, valid input is rotation to change selection or press to confirm selection and enter edit mode
    if (isChooseState(userInputState)) {
        //on press, move from choose state to edit state for the selected variable
        if (pressed) {
            bool changedStatus = false;
            //special case if we're choosing the status, since that doesn't have a single value but rather multiple options based on the chargersm state
            if (userInputState == Edit_Choose_Status) {
                const uint8_t *options = get_UI_options(chargersm);
                if (options && options[0] > 0) {
                    currentEditValue = 1;
                    uiStatusMsg = statusChangeMessages[options[currentEditValue]];
                    changedStatus = true;
                } else {
                    currentEditValue = 0;
                    uiStatusMsg = statusChangeMessages[NO_OPTIONS];
                    changedStatus = true;
                }
            } else {
                currentEditValue = userVars[userInputState];
            }

            display.invertInfo(infoFromState(userInputState), true);
            userInputState = static_cast<UserInputState>(userInputState + NUM_EDIT_PARAMS);
            return changedStatus;
        }

        //if we're just choosing which value to edit, any rotation input just changes the selection without modifying the value, 
        //so we can return early after updating the box highlight
        if (net_enc_delta != 0) {
            display.boxInfo(infoFromState(userInputState), false);
            userInputState = static_cast<UserInputState>(
                mod_pos(static_cast<int8_t>(userInputState) + net_enc_delta, NUM_EDIT_PARAMS));
            display.boxInfo(infoFromState(userInputState), true);
        }
        return false;
    }
    //if we're editing a value, rotation input changes the value and press confirms the change and goes back to choose state
    if (isEditState(userInputState)) {
        //on press, save the edited value and return to choose state
        if (pressed) {
            uint8_t selectedIdx = infoFromState(userInputState);
            //special case for status since we need to convert from the selected index to the actual status path based on the chargersm state
            if (userInputState == Edit_Status) {
                userVars[selectedIdx] = selectedStatusPathFromIndex(currentEditValue, chargersm);
            } else {
                userVars[selectedIdx] = clamp_user_var(currentEditValue, static_cast<InfoType>(selectedIdx));
            }

            display.invertInfo(infoFromState(userInputState), false);
            userInputState = static_cast<UserInputState>(userInputState - NUM_EDIT_PARAMS);
            display.boxInfo(infoFromState(userInputState), true);

            uiStatusMsg = nullptr; // clear status message on edit, since it may no longer be relevant after the change and we don't want to show an outdated one. Will be updated in the main loop after processing the change.
            return true;
        }
        //
        if (net_enc_delta != 0) {
            //special case for status since we need to convert from the selected index to the actual status path based on the chargersm state
            if (userInputState == Edit_Status) {
                const uint8_t *options = get_UI_options(chargersm);
                if (options && options[0] > 0) {
                    uint8_t numOptions = options[0];
                    currentEditValue = mod_pos(static_cast<int8_t>(currentEditValue-1) + net_enc_delta, numOptions) + 1;
                    uiStatusMsg = statusChangeMessages[options[currentEditValue]];
                } else {
                    uiStatusMsg = statusChangeMessages[NO_OPTIONS];
                }
                return true;
            } else {
                currentEditValue += net_enc_delta;
                currentEditValue = clamp_user_var(currentEditValue, static_cast<InfoType>(infoFromState(userInputState)));
                display.setInfo(infoFromState(userInputState), currentEditValue);
            }
        }
    }

    return false;
}

//FAULT DISPLAYING
//from chargersm.h:
//error_flags:
//bits 0-4: elcon faults (see datasheet)
//bit 5: Elcon CAN Timeout
//bit 6: TBP Not Ready (bsmState not 5)
//bit 7: BSM Fault (bsmState is 0xF, which is the fault state for the BSM)
//bit 8: BSM CAN Timeout
//bit 9: SDC
//bit 10: Cell overvoltage

const char* faultMsgs[] = {
   //ABCDEFGHIJK
    "E HARDWARE",
    "E TEMP",
    "E INPUT",
    "E BATTERY",
    "E COMMS",
    "E CAN T\\O",
    "TBP NOT RDY",
    "BSM FAULT",
    "BSM CAN T\\O",
    "SDC",
    "CELL O\\V"
};

#define FAULT_DISPLAY_TIME 1500 //ms to display each fault message
void setFaultMsg(Display& disp, chargersm_obj *me){
    static uint8_t last_fault_index = 0xFF;
    static uint16_t last_fault_mask = 0;
    static uint32_t last_fault_msg_change_tick = 0;
    
    static uint8_t fault_num = 1;

    //count number of faults
    uint8_t num_faults = 0;
    for (int i = 0; i < NUM_FAULTS; i++){
        if (me->error_flags & (1 << i)){
            num_faults++;
        }
    }

    //unknown error
    if (num_faults == 0){
        disp.setStatus("UNKNOWN ERROR");
        return;
    }

    //if number of faults has changed, or the current fault index is no longer valid, reset to first fault
    if (me->error_flags != last_fault_mask){
        last_fault_index = 0xFF;
        last_fault_mask = me->error_flags;
        fault_num = 1;
    }

    //if it's time to change the fault message, update the index
    if (HAL_GetTick() - last_fault_msg_change_tick >= FAULT_DISPLAY_TIME){
        do { //find the next active fault
            //for some reason, doing this in two step fixed an indexing bug
            //should probably investigate further
            last_fault_index = (last_fault_index + 1);
            last_fault_index %= NUM_FAULTS;
        } while (!(me->error_flags & (1 << last_fault_index)));
        
        disp.setStatus("ERR");
        disp.setStatus(faultMsgs[last_fault_index], 4, true); // display fault message

        disp.setStatus("[ \\ ]", 16, false); // display fault count in brackets
        char i[2] = {static_cast<char>(fault_num + '0'), '\0'}; // convert fault_num to char
        disp.setStatus(i, 17, false); // display fault number in brackets

        i[0] = static_cast<char>(num_faults + '0');
        disp.setStatus(i, 19, false); // display total number of faults

        last_fault_msg_change_tick = HAL_GetTick();
        fault_num = (fault_num % num_faults) + 1;
    }


}

int16_t timeDisplay(uint32_t timeMs){
    if (timeMs >= 3600000) {
        // display in HH:MM format
        uint32_t totalSeconds = timeMs / 1000;
        uint16_t hours = totalSeconds / 3600;
        uint16_t minutes = (totalSeconds % 3600) / 60;
        return (hours * 100) + minutes;
    } else {
        // display in MM:SS format
        uint32_t totalSeconds = timeMs / 1000;
        uint16_t minutes = totalSeconds / 60;
        uint16_t seconds = totalSeconds % 60;
        return (minutes * 100) + seconds;
    }
}

//update display with all required info
void updateDisplayInfo(Display& disp, const CANInfo& info, chargersm_obj &chargersm){
    //if things get slow, it could be worth only updating things that changed
    disp.setInfo(TEMP_HIGH_CELL, info.maxTempCell);
    disp.setInfo(TEMP_HIGH_VALUE, cell_temperature_conversion(info.maxTempVal));
    disp.setInfo(TEMP_LOW_CELL, info.minTempCell);
    disp.setInfo(TEMP_LOW_VALUE, cell_temperature_conversion(info.minTempVal));
    disp.setInfo(TEMP_NOM, cell_temperature_conversion(info.nomTempVal));

    disp.setInfo(VOLTAGE_HIGH_CELL, info.maxVoltCell);
    disp.setInfo(VOLTAGE_HIGH_VALUE, cell_voltage_conversion(info.maxVoltVal));
    disp.setInfo(VOLTAGE_LOW_CELL, info.minVoltCell);
    disp.setInfo(VOLTAGE_LOW_VALUE, cell_voltage_conversion(info.minVoltVal));
    disp.setInfo(VOLTAGE_NOM, cell_voltage_conversion(info.nomVoltVal));

    disp.setInfo(CHARGE_PERCENT, pack_soc_conversion(info.SOC));
    disp.setInfo(PACK_VOLTAGE, pack_voltage_conversion(info.packVoltage));
    
    //for time remaining, we can only display if we're currently charging and have a valid estimate
    if (chargersm.chargerState == AUTOMATIC || chargersm.chargerState == MANUAL) {
        disp.setInfo(TIME_PAST, timeDisplay(HAL_GetTick() - chargersm.timeSinceCharge));
        
        //DO SOMETHING WITH SOC
        disp.setInfo(TIME_REMAINING, 1234); 
    } else {
        //not changing could be nice for data collection
        //disp.setInfo(TIME_PAST, 0);
        disp.setInfo(TIME_REMAINING, 0);
    }
}
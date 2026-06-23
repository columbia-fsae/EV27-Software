#include "rotary_encoder.hpp"

RotaryEncoder::RotaryEncoder(GPIO_TypeDef* A_PORT, uint16_t A_PIN,
                             GPIO_TypeDef* B_PORT, uint16_t B_PIN,
                             GPIO_TypeDef* S_PORT, uint16_t S_PIN)
    : A_PORT(A_PORT), A_PIN(A_PIN),
      B_PORT(B_PORT), B_PIN(B_PIN),
      S_PORT(S_PORT), S_PIN(S_PIN),
      input_buffer(0)
{}


// Valid CW  transitions: 00→01→11→10→00
// Valid CCW transitions: 00→10→11→01→00
// Used to detect invalid transitions (bounces) and filter them out
const int8_t RotaryEncoder::TRANSITION_TABLE[16] = {
//  00  01  10  11  <- new state
    0, -1,  1,  0,  // old state 00
    1,  0,  0, -1,  // old state 01
   -1,  0,  0,  1,  // old state 10
    0,  1, -1,  0   // old state 11
};

void RotaryEncoder::rotationCallback() {
    uint8_t new_state = (HAL_GPIO_ReadPin(A_PORT, A_PIN) << 1)
                       | HAL_GPIO_ReadPin(B_PORT, B_PIN);
    int8_t delta = TRANSITION_TABLE[(ab_state << 2) | new_state];
    ab_state = new_state;
    
    //I realized this check sucked, and instead always updating the counter is better
    /*
    // Update threshold counter with direction, reset if direction changes or if invalid transition
    if ((threshold_ctr & (1<<8)) != (delta & (1<<8))) {
        threshold_ctr = delta;
    } else {
        threshold_ctr += delta;
    }
    */

    threshold_ctr += delta;

    //check the counter agains the threshold and add to buffer if threshold is crossed, then reset counter.
    if (threshold_ctr >= DETENT_THRESHOLD) {
        input_buffer = input_buffer | (0b01 << (numActions * 2)); // CW
        numActions++;
        threshold_ctr = 0;
    } else if (threshold_ctr <= -DETENT_THRESHOLD) {
        input_buffer = input_buffer | (0b10 << (numActions * 2)); // CCW
        numActions++;
        threshold_ctr = 0;
    }
    // delta == 0 means invalid/bounce transition, ignore it
}

void RotaryEncoder::switchCallback() {
    //for debounce
    if (HAL_GetTick() - lastButtonTick > 1000){
        input_buffer = input_buffer | (0b11 << (numActions * 2)); // Button pressed
        numActions++;
        lastButtonTick = HAL_GetTick();
    }
}

void RotaryEncoder::resetBuffer(){
    input_buffer = 0;
    numActions = 0;
}

ROTARY_ENCODER_BUFFER_TYPE RotaryEncoder::getBuffer() {
    ROTARY_ENCODER_BUFFER_TYPE temp = input_buffer;
    resetBuffer();
    return temp;
}

ROTARY_ENCODER_BUFFER_TYPE RotaryEncoder::peekBuffer() {
    return input_buffer; // Return buffer without clearing
}

bool RotaryEncoder::processBuffer(int8_t &out_delta){
    //to prevent interrupts from messing with this while processing
    ROTARY_ENCODER_BUFFER_TYPE temp_buffer = getBuffer();
    out_delta = 0;
    while (temp_buffer != 0){
        if ((temp_buffer&0b11) == 0b01){ //CW
            out_delta++;
        } else if ((temp_buffer&0b11) == 0b10){ //CCW
            out_delta--;
        } else if ((temp_buffer&0b11) == 0b11){ //button press, confirm choose and move to edit state
            return true;
        }
        temp_buffer = temp_buffer >> 2;
    }
    return false;
}

// RotaryEncoder.hpp
#ifndef ROTARY_ENCODER_HPP
#define ROTARY_ENCODER_HPP

#include "stm32f0xx_hal.h"
#include <stdint.h>

#define DETENT_THRESHOLD 4

typedef uint32_t ROTARY_ENCODER_BUFFER_TYPE;

class RotaryEncoder {
public:
    // Constructor with pin configurations
    RotaryEncoder(GPIO_TypeDef* A_PORT, uint16_t A_PIN,
                  GPIO_TypeDef* B_PORT, uint16_t B_PIN,
                  GPIO_TypeDef* S_PORT, uint16_t S_PIN);
    
    void rotationCallback();
    void switchCallback();
    void resetBuffer();
    ROTARY_ENCODER_BUFFER_TYPE getBuffer();
    ROTARY_ENCODER_BUFFER_TYPE peekBuffer();
    
    bool processBuffer(int8_t &out_delta);
    
private:
    // Pin configurations
    GPIO_TypeDef* A_PORT; uint16_t A_PIN;
    GPIO_TypeDef* B_PORT; uint16_t B_PIN;
    GPIO_TypeDef* S_PORT; uint16_t S_PIN  ;
    
    // State variables
    volatile ROTARY_ENCODER_BUFFER_TYPE input_buffer;
    volatile uint8_t ab_state = 0; // 2 bits for A and B state
    volatile int8_t threshold_ctr = 0; //counts threshold crossings to filter out noise
    volatile uint8_t numActions = 0; //number of valid actions in buffer

    //transition table for rotary encoder state changes
    static const int8_t TRANSITION_TABLE[16];
    
    uint32_t lastButtonTick = 0;
};

#endif
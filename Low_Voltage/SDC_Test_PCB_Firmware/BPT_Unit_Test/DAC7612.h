#ifndef DAC7612_H    // Include guard
#define DAC7612_H

#include <Arduino.h>  // Include Arduino functions if needed

//Define pins
#define CS         17
#define CLK        15
#define COPI       16
#define LOADDACS   14

// Function declaration
void setVoltage(float voltageBPT, float voltageCT);

#endif
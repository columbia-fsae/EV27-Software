/*
SDC LED should be on.
****Commencing BMS Unit Test****
Setting BMS to 0V...
Expected BMS 0V, BMS LED on, and SDC LED off.
Received BMS: 0V

Setting BMS to 24V...
Expected BMS 5V, BMS LED off, and SDC LED on.
Received BMS: 5V

Tests passed!
*/

#include "DAC7612.h"

const int BMS = 19;
const int IMD = 22;
const int RESET = 23;
const int SDC_OUT = 4; 
const int DUAL_OK = 9;
const int BPT_SC_OK = 5;
const int BPT_OC_OK = 6;
const int CT_SC_OK = 12;
const int CT_OC_OK = 11;

void setup() {
  //initialize pins
  Serial.begin(9600);
  //BMS, IMD pinouts
  pinMode(BMS, OUTPUT);
  pinMode(IMD, OUTPUT);
  //BPT, CT pinouts
  pinMode(CS, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(COPI, OUTPUT);
  pinMode(LOADDACS, OUTPUT);
  // Inputs
  pinMode(DUAL_OK, INPUT);
  pinMode(SDC_OUT, INPUT);
  pinMode(BPT_SC_OK, INPUT);
  pinMode(BPT_OC_OK, INPUT);
  pinMode(CT_SC_OK, INPUT);
  pinMode(CT_OC_OK, INPUT);

  // set signals to DEFAULT_POSITION
  digitalWrite(BMS, HIGH); // 24V
  digitalWrite(IMD, HIGH); // 24V
  setVoltage(2.0, 2.0); // BPT, CT

  // wait for any key to be pressed to start test
  while (Serial.available() == 0) {}
  Serial.read(); // read and discard pressed key

  
}

void loop() {
  Serial.println("Starting range testing.");
  setVoltage(0.0,2.0);
  delay(1000);
  setVoltage(1.0,2.0);
  delay(1000);
  setVoltage(2.0,2.0);
  delay(1000);
  setVoltage(3.0,2.0);
  delay(1000);
  setVoltage(4.0,2.0);
  delay(1000);
  setVoltage(2.0,0.0);
  delay(1000);
  setVoltage(2.0,1.0);
  delay(1000);
  setVoltage(2.0,2.0);
  delay(1000);
  setVoltage(2.0,3.0);
  delay(1000);
  setVoltage(2.0,4.0);
  delay(1000);
}

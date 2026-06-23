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
const int CT_SC_OK = 12;
const int CT_OC_OK = 11;
bool passed = true;

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
  pinMode(SDC_OUT, INPUT);
  pinMode(CT_SC_OK, INPUT);
  pinMode(CT_OC_OK, INPUT);

  //set signals to DEFAULT_POSITION
  digitalWrite(BMS, HIGH); // 24V
  digitalWrite(IMD, HIGH); // 24V
  setVoltage(2.0, 2.0); // BPT, CT

  // wait for any key to be presesd to start test
  Serial.println("Press any key to start CT test:");
  while (Serial.available() == 0) {}
  Serial.read(); // read and discard pressed key

  // ****Begin BMS Testing****
  Serial.println("SDC LED should be on.");
  if (digitalRead(SDC_OUT) == LOW){
    passed = false; 
    Serial.println("ERROR: SDC is faulting in initial state.");
  } else {
    Serial.println("SDC is high in initial state.");
  }
  Serial.println("SDC LED should be on.");
  delay(2000);

  Serial.println("****Commencing Dual Unit Test****");

  //Set CT Low
  Serial.println("Setting Only CT Low (CT=0V)");
  setVoltage(2.0, 0);
  Serial.println("SDC_LED should be off");
  delay(2000);
  Serial.println("Expected SDC_OUT: 0V, Expected CT_OC_OK: 0V");
  if(digitalRead(SDC_OUT) == 1 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 1){
      passed = false; 
  }
  Serial.print(digitalRead(SDC_OUT) ? "Recieved SDC_OUT: 5V, " : "Recieved SDC_OUT: 0V, ");
  Serial.print(digitalRead(CT_SC_OK) ? "Recieved CT_SC_OK: 5V, " : "Recieved CT_SC_OK: 0V, ");
  Serial.print(digitalRead(CT_OC_OK) ? "Recieved CT_OC_OK: 5V" : "Recieved CT_OC_OK: 0V");
  Serial.println("");

  //CT Low Latch Check
  Serial.println("Checking latch, Setting back to default (BPT=2V, CT=2V)");
  setVoltage(2.0, 2.0);
  Serial.println("SDC_LED should be off");
  delay(2000);
  Serial.println("Expected SDC_OUT: 0V, Expected CT_OC_OK: 5V");
  if(digitalRead(SDC_OUT) == 1 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0){
      passed = false; 
  }
  Serial.print(digitalRead(SDC_OUT) ? "Recieved SDC_OUT: 5V, " : "Recieved SDC_OUT: 0V, ");
  Serial.print(digitalRead(CT_SC_OK) ? "Recieved CT_SC_OK: 5V, " : "Recieved CT_SC_OK: 0V, ");
  Serial.print(digitalRead(CT_OC_OK) ? "Recieved CT_OC_OK: 5V" : "Recieved CT_OC_OK: 0V");
  Serial.println("");

  //Reset Check
  digitalWrite(RESET, HIGH);
  delay(200);
  digitalWrite(RESET,LOW);
  Serial.println("Checking Reset (CT=2.0V)");
  setVoltage(2.0, 2.0);
  Serial.println("SDC_LED should be on");
  delay(2000);
  Serial.println("Expected SDC_OUT: 5V, Expected CT_OC_OK: 5V");
  if(digitalRead(SDC_OUT) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0){
      passed = false; 
  }
  Serial.print(digitalRead(SDC_OUT) ? "Recieved SDC_OUT: 5V, " : "Recieved SDC_OUT: 0V, ");
  Serial.print(digitalRead(CT_SC_OK) ? "Recieved CT_SC_OK: 5V, " : "Recieved CT_SC_OK: 0V, ");
  Serial.print(digitalRead(CT_OC_OK) ? "Recieved CT_OC_OK: 5V" : "Recieved CT_OC_OK: 0V");
  Serial.println("");

  // pass or fail
  if(passed){Serial.println("Tests passed!");}
  else{Serial.println("Tests failed :(");}
}

void loop() {
}

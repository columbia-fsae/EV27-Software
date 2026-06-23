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
const int BPT_SC_OK = 5;
const int BPT_OC_OK = 6;
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
  pinMode(BPT_SC_OK, INPUT);
  pinMode(BPT_OC_OK, INPUT);

  // set signals to DEFAULT_POSITION
  digitalWrite(BMS, HIGH); // 24V
  digitalWrite(IMD, HIGH); // 24V
  setVoltage(2.0, 2.0); // BPT, CT

  // wait for any key to be presesd to start test
  Serial.println("Press any key to start BPT test:");
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

  Serial.println("****Commencing CT Unit Test****");

  //Set BPT Low
  Serial.println("Setting Only BPT Low (BPT=0V, CT=2V)");
  setVoltage(0, 2);
  Serial.println("SDC_LED should be off");
  delay(2000);
  Serial.println("Expected SDC_OUT: 0V, Expected BPT_SC_OK: 5V, Expected BPT_OC_OK: 0V");
  if(digitalRead(SDC_OUT) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 1){
      passed = false; 
  }
  Serial.print(digitalRead(SDC_OUT) ? "Recieved SDC_OUT: 5V, " : "Recieved SDC_OUT: 0V, ");
  Serial.print(digitalRead(BPT_SC_OK) ? "Recieved BPT_SC_OK: 5V, " : "Recieved BPT_SC_OK: 0V, ");
  Serial.print(digitalRead(BPT_OC_OK) ? "Recieved BPT_OC_OK: 5V" : "Recieved BPT_OC_OK: 0V");
  Serial.println("");

  //BPT Low Latch Check
  Serial.println("Checking BPT latch (BPT=2V, BPT=2V)");
  setVoltage(2.0, 2.0);
  Serial.println("SDC_LED should be off");
  delay(2000);
  Serial.println("Expected SDC_OUT: 0V, Expected BPT_SC_OK: 5V, Expected BPT_OC_OK: 5V");
  if(digitalRead(SDC_OUT) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0){
      passed = false; 
  }
  Serial.print(digitalRead(SDC_OUT) ? "Recieved SDC_OUT: 5V, " : "Recieved SDC_OUT: 0V, ");
  Serial.print(digitalRead(BPT_SC_OK) ? "Recieved BPT_SC_OK: 5V, " : "Recieved BPT_SC_OK: 0V, ");
  Serial.print(digitalRead(BPT_OC_OK) ? "Recieved BPT_OC_OK: 5V" : "Recieved BPT_OC_OK: 0V");
  Serial.println("");

  //Reset Check
  digitalWrite(RESET, HIGH);
  delay(200);
  digitalWrite(RESET,LOW);
  Serial.println("Checking Reset (BPT=2.0V)");
  setVoltage(2.0, 2.0);
  Serial.println("SDC_LED should be on");
  delay(2000);
  Serial.println("Expected SDC_OUT: 5V, Expected BPT_SC_OK: 5V, Expected BPT_OC_OK: 5V");
  if(digitalRead(SDC_OUT) == 0 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0){
      passed = false; 
  }
  Serial.print(digitalRead(SDC_OUT) ? "Recieved SDC_OUT: 5V, " : "Recieved SDC_OUT: 0V, ");
  Serial.print(digitalRead(BPT_SC_OK) ? "Recieved BPT_SC_OK: 5V, " : "Recieved BPT_SC_OK: 0V, ");
  Serial.print(digitalRead(BPT_OC_OK) ? "Recieved BPT_OC_OK: 5V" : "Recieved BPT_OC_OK: 0V");
  Serial.println("");

  // pass or fail
  if(passed){Serial.println("Tests passed!");}
  else{Serial.println("Tests failed :(");}
}

void loop() {
}

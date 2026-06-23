/* DESIRED OUTPUT

SDC LED should turn on.
Press any key to start BMS test:
SDC is HIGH in initial state.

****Commencing BMS Unit Test****
Setting BMS to 0V...
Expected BMS 0V, BMS LED on, SDC 0V, and SDC LED off.
Received BMS: 0V
Received SDC: 0V

Setting BMS to 24V...
Expected BMS 5V, BMS LED off, SDC 5V, and SDC LED on.
Received BMS: 5V
Received SDC: 5V

Tests passed!
*/

#include "DAC7612.h"

const int BMS = 19;
const int IMD = 22;
const int RESET = 23;
const int BMS_DASH_LED = 9;
const int SDC_OUT = 4;
bool passed = true;

void setup() {
  //initialize pins
  Serial.begin(9600);
  pinMode(BMS, OUTPUT);
  pinMode(IMD, OUTPUT);
  pinMode(CS, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(COPI, OUTPUT);
  pinMode(LOADDACS, OUTPUT);
  pinMode(BMS_DASH_LED, INPUT);
  pinMode(SDC_OUT, INPUT);

  // set signals to DEFAULT_POSITION
  digitalWrite(BMS, HIGH); // 24V
  digitalWrite(IMD, HIGH); // 24V
  setVoltage(2.0, 2.0); // BPT, CT

  Serial.println("SDC LED should turn on.");

  // wait for any key to be presesd to start test
  Serial.println("Press any key to start BMS test:");
  while (Serial.available() == 0) {}
  Serial.read(); // read and discard pressed key

  // ****Begin BMS Testing****
  if(digitalRead(SDC_OUT) == LOW){
    Serial.println("ERROR: SDC is faulting in initial state.");
    passed = false;
  }
  else{
    Serial.println("SDC is HIGH in initial state.");
  }
  Serial.println("****Commencing BMS Unit Test****");

  // 0V BMS
  Serial.println("Setting BMS to 0V...");
  digitalWrite(BMS, LOW); // 0V signal
  delay(1000); // wait 1 sec
  int BMS_OUTPUT = 5 * digitalRead(BMS_DASH_LED); // read BMS output, 5 or 0
  int SDC_OUTPUT = 5 * digitalRead(SDC_OUT); // read SDC output, 5 or 0
  if(BMS_OUTPUT != 0 || SDC_OUTPUT != 0) {
    passed = false;
    Serial.println("ERROR: Failed at BMS 0V.");
  }
  Serial.println("Expected BMS 0V, BMS LED on, SDC 0V, and SDC LED off.");
  Serial.print("Received BMS: ");
  Serial.print(BMS_OUTPUT);
  Serial.println("V\n");
  Serial.print("Received SDC: ");
  Serial.print(SDC_OUTPUT);
  Serial.println("V\n");

  delay(2000);

  // 24V BMS
  Serial.println("Setting BMS to 24V...");
  digitalWrite(BMS, HIGH); // 24V
  digitalWrite(RESET, HIGH); // reset
  delay(1000); // wait 1 sec
  BMS_OUTPUT = 5 * digitalRead(BMS_DASH_LED); // read BMS output, 5 or 0
  SDC_OUTPUT = 5 * digitalRead(SDC_OUT); // read SDC output, 5 or 0
  if(BMS_OUTPUT != 5 || SDC_OUTPUT != 5) {
    passed = false;
    Serial.println("ERROR: Failed at BMS 24V.");
  }
  Serial.println("Expected BMS 5V, BMS LED off, SDC 5V, and SDC LED on.");
  Serial.print("Received BMS: ");
  Serial.print(BMS_OUTPUT);
  Serial.println("V\n");
  Serial.print("Received SDC: ");
  Serial.print(SDC_OUTPUT);
  Serial.println("V\n");

  // pass or fail
  if(passed){Serial.println("Tests passed!");}
  else      {Serial.println("Tests failed :(");}
}

void loop() {
}

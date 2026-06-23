#include "DAC7612.h" // import code for dac

// pins for arduino outputs
const int BMS = 19;
const int IMD = 22;
const int RESET = 23;

// pins for arduino inputs
const int BMS_DASH_LED = 9;
const int IMD_DASH_LED = 8;
const int BSPD_DASH_LED = 7;
const int SDC_OUT = 13;
const int DUAL_OK = 4;
const int BPT_SC_OK = 5;
const int BPT_OC_OK = 6;
const int CT_SC_OK = 12;
const int CT_OC_OK = 11;

bool passed; // variable to monitor whether tests are being passed or not
bool Npassed; // variable to monitor whether all N cycles passed in the Ncyle test

void setup() {
  Serial.begin(9600);

  // Output initialization
  pinMode(BMS, OUTPUT);
  pinMode(IMD, OUTPUT);
  pinMode(CS, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(COPI, OUTPUT);
  pinMode(LOADDACS, OUTPUT);
  pinMode(RESET, OUTPUT);

  // Input initialization
  pinMode(SDC_OUT, INPUT);
  pinMode(BMS_DASH_LED, INPUT);
  pinMode(IMD_DASH_LED, INPUT);
  pinMode(BSPD_DASH_LED, INPUT);
  pinMode(DUAL_OK, INPUT);
  pinMode(SDC_OUT, INPUT);
  pinMode(BPT_SC_OK, INPUT);
  pinMode(BPT_OC_OK, INPUT);
  pinMode(CT_SC_OK, INPUT);
  pinMode(CT_OC_OK, INPUT);

  defaultPosition();
  reset();
}

void loop() {
  // Serial receive handling: receive serial input from python code on computer
  // receive instructions on which tests to run

  if (Serial.available() > 0) {

    char c = Serial.read();
    if (c == '\n' || c == '\r') return;
    
    if (c == 'm') { // bms unit test
      bmsTest();
    } 
    else if (c== 'p'){ // bpt unit test
      bptTest();
    }
    else if (c == 'c'){ // ct unit test
      ctTest();
    }
    else if (c == 'd'){ // dual unit test
      dualTest();
    }
    else if (c == 'i'){ // imd unit test
      imdTest();
    }
    else if (c == 'n'){ // n cycle test
      Npassed = true;
      int n = Serial.parseInt(); // number of iterations
      ncycle(1, n);
    }
  }
}

void defaultPosition(){
  // set signals to DEFAULT_POSITION
  digitalWrite(RESET, LOW);
  digitalWrite(BMS, HIGH); // 24V
  digitalWrite(IMD, HIGH); // 24V
  setVoltage(2.0, 2.0); // BPT, CT
  delay(500);
}

void reset(){
  digitalWrite(RESET, HIGH);
  delay(500);
  digitalWrite(RESET, LOW);
  delay(500);
}

void printInputs(int sdc, int bms, int imd, int bpt_oc, int bpt_sc, int ct_oc, int ct_sc, int bspd, int dual){

  Serial.print(F("Expected SDC_OUT: "));
  Serial.print(sdc);
  Serial.print(F("V (SDC LED "));
  if(sdc == 0){Serial.print(F("off)"));}
  else        {Serial.print(F("on)"));}
  Serial.println(digitalRead(SDC_OUT) ? "\t\tReceived SDC_OUT: 5V" : "\t\tReceived SDC_OUT: 0V");

  Serial.print(F("Expected BMS_DASH_LED: "));
  Serial.print(bms);
  Serial.print(F("V (BMS LED "));
  if(bms == 0){Serial.print(F("off)"));}
  else        {Serial.print(F("on)"));} 
  Serial.println(digitalRead(BMS_DASH_LED) ? "\t\tReceived BMS_DASH_LED: 5V" : "\t\tReceived BMS_DASH_LED: 0V");
  
  Serial.print(F("Expected IMD_DASH_LED: "));
  Serial.print(imd);
  Serial.print(F("V (IMD LED "));
  if(imd == 0){Serial.print(F("off)"));}
  else        {Serial.print(F("on)"));}
  Serial.println(digitalRead(IMD_DASH_LED) ? "\t\tReceived IMD_DASH_LED: 5V" : "\t\tReceived IMD_DASH_LED: 0V");

  Serial.print(F("Expected BPT_OC_OK: "));
  Serial.print(bpt_oc);
  Serial.print(F("V (BPT_OC_OK LED "));
  if(bpt_oc == 0){Serial.print(F("on)"));}
  else        {Serial.print(F("off)"));}
  Serial.println(digitalRead(BPT_OC_OK) ? "\tReceived BPT_OC_OK: 5V" : "\tReceived BPT_OC_OK: 0V");
  
  Serial.print(F("Expected BPT_SC_OK: "));
  Serial.print(bpt_sc);
  Serial.print(F("V (BPT_SC_OK LED "));
  if(bpt_sc == 0){Serial.print(F("on)"));}
  else        {Serial.print(F("off)"));}
  Serial.println(digitalRead(BPT_SC_OK) ? "\tReceived BPT_SC_OK: 5V" : "\tReceived BPT_SC_OK: 0V");

  Serial.print(F("Expected CT_OC_OK: "));
  Serial.print(ct_oc);
  Serial.print(F("V (CT_OC_OK LED "));
  if(ct_oc == 0){Serial.print(F("on)  "));}
  else        {Serial.print(F("off)  "));}
  Serial.println(digitalRead(CT_OC_OK) ? "\tReceived CT_OC_OK: 5V" : "\tReceived CT_OC_OK: 0V");

  Serial.print(F("Expected CT_SC_OK: "));
  Serial.print(ct_sc);
  Serial.print(F("V (CT_SC_OK LED "));
  if(ct_sc == 0){Serial.print(F("on)  "));}
  else        {Serial.print(F("off)  "));}
  Serial.println(digitalRead(CT_SC_OK) ? "\tReceived CT_SC_OK: 5V" : "\tReceived CT_SC_OK: 0V");

  Serial.print(F("Expected BSPD_DASH_LED: "));
  Serial.print(bspd);
  Serial.print(F("V (BSPD LED "));
  if(bspd == 0){Serial.print(F("off)"));}
  else        {Serial.print(F("on)"));}
  Serial.println(digitalRead(BSPD_DASH_LED) ? "\tReceived BSPD_DASH_LED: 5V" : "\tReceived BSPD_DASH_LED: 0V");
  
  Serial.print(F("Expected DUAL_OK: "));
  Serial.print(dual);
  Serial.print(F("V"));
  Serial.println(digitalRead(DUAL_OK) ? "\t\t\t\tReceived DUAL_OK: 5V" : "\t\t\t\tReceived DUAL_OK: 0V");
}

void bmsTest() {
  defaultPosition();
  passed = true;

  Serial.println(F("SDC LED should turn on."));

  // ****Begin BMS Testing****
  if(digitalRead(SDC_OUT) == LOW){
    Serial.println(F("ERROR: SDC is faulting in initial state."));
    passed = false;
  }
  else{
    Serial.println(F("SDC is HIGH in initial state."));
  }

  Serial.println(F("\n****Commencing BMS Unit Test****"));

  // 0V BMS, BMS fault
  Serial.println(F("Checking BMS low (BMS=0V)"));
  digitalWrite(BMS, LOW); // 0V signal
  delay(500);
  if(digitalRead(SDC_OUT) == 1 ||
    digitalRead(BMS_DASH_LED) == 0 || 
    
    digitalRead(IMD_DASH_LED) == 1 ||
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0) {
    passed = false;
    Serial.println(F("ERROR: Failed at BMS 0V."));
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 5, 0, 5, 5, 5, 5, 0, 5);

  delay(2000);

  // test latch, 24V BMS, BMS still fault, SDC still off
  Serial.println(F("\nChecking BMS latch (BMS=24V)"));
  digitalWrite(BMS, HIGH); // 24V
  delay(500);
  if(digitalRead(BMS_DASH_LED) == 0 || 
    digitalRead(SDC_OUT) == 1 ||
    
    digitalRead(IMD_DASH_LED) == 1 ||
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0) {
    passed = false;
    Serial.println(F("ERROR: Did not latch BMS fault (24V BMS but not yet reset)."));
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 5, 0, 5, 5, 5, 5, 0, 5);

  delay(2000);

  // 24V BMS, reset, BMS no fault, SDC on
  Serial.println(F("\nChecking Reset (BMS stays at 24V)"));
  reset();
  if(digitalRead(BMS_DASH_LED) == 1 || 
    digitalRead(SDC_OUT) == 0 || 
    
    digitalRead(IMD_DASH_LED) == 1 ||
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0) {
    passed = false;
    Serial.println(F("ERROR: Failed at BMS 24V."));
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(5, 0, 0, 5, 5, 5, 5, 0, 5);

  // pass or fail
  if(passed){Serial.println(F("\nTests passed!"));}
  else      {Serial.println(F("\nTests failed :("));}
}

void imdTest(){
  defaultPosition();
  passed = true;

  Serial.println(F("SDC LED should turn on."));

  // ****Begin IMD Testing****
  // autocheck if SDC is high
  if(digitalRead(SDC_OUT) == LOW){
    Serial.println(F("ERROR: SDC is faulting in initial state."));
    passed = false;
  }
  else{
    Serial.println(F("SDC is HIGH in initial state."));
  }

  Serial.println(F("\n****Commencing IMD Unit Test****"));

  // 0V IMD
  Serial.println(F("Checking IMD low (IMD=0V)"));
  digitalWrite(IMD, LOW); // 0V signal
  delay(500);
  if(digitalRead(IMD_DASH_LED) == 0 || 
    digitalRead(SDC_OUT) == 1 || 
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0) {
    passed = false;
    Serial.println(F("ERROR: Failed at IMD 0V."));
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 0, 5, 5, 5, 5, 5, 0, 5);

  delay(2000);

  // test latch, 24V IMD, IMD still fault, SDC still off
  Serial.println(F("\nChecking IMD latch (IMD=24V)"));
  digitalWrite(IMD, HIGH); // 24V
  delay(500);
  if(digitalRead(IMD_DASH_LED) == 0 || 
    digitalRead(SDC_OUT) == 1 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0) {
    passed = false;
    Serial.println(F("ERROR: Did not latch IMD fault (24V IMD but not yet reset)."));
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 0, 5, 5, 5, 5, 5, 0, 5);

  delay(2000);

  // 24V IMD, IMD no fault, SDC on
  Serial.println(F("\nChecking reset (IMD stays at 24V)"));
  reset();
  if(digitalRead(IMD_DASH_LED) == 1 || 
    digitalRead(SDC_OUT) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0) {
    passed = false;
    Serial.println(F("ERROR: Failed at IMD 24V."));
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(5, 0, 0, 5, 5, 5, 5, 0, 5);

  // pass or fail
  if(passed){Serial.println(F("\nTests passed!"));}
  else      {Serial.println(F("\nTests failed :("));}
}

void bptTest(){
  // set signals to DEFAULT_POSITION
  defaultPosition();
  passed = true;

  Serial.println(F("SDC LED should be on."));

  // ****Begin BPT Testing****
  if (digitalRead(SDC_OUT) == LOW){
    passed = false; 
    Serial.println("ERROR: SDC is faulting in initial state.");
  } else {
    Serial.println("SDC is HIGH in initial state.");
  }

  Serial.println("\n****Commencing BPT Unit Test****");

  //Set BPT low, BPT_OC_OK LED on, BSPD LED on, SDC off
  Serial.println("Setting only BPT low (BPT=0V, CT=2V)");
  setVoltage(0, 2.0);
  delay(500);
  if(digitalRead(SDC_OUT) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 1 ||
    digitalRead(BSPD_DASH_LED) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0){
    passed = false;
    Serial.println("ERROR: Failed at BPT 0V.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 0, 0, 0, 5, 5, 5, 5, 5);

  delay(2000);

  //BPT Low Latch Check, BPT_OC_OK high now, still faulting, SDC still off
  Serial.println("\nChecking BPT latch (BPT=2V, CT=2V)");
  setVoltage(2.0, 2.0);
  delay(500);
  if(digitalRead(SDC_OUT) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(BSPD_DASH_LED) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0){
    passed = false; 
    Serial.println("ERROR: Failed to latch BPT.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 0, 0, 5, 5, 5, 5, 5, 5);

  delay(2000);

  //Reset check, in default mode, no faults, SDC on
  Serial.println("\nChecking reset (BPT and CT stay at 2V)");
  reset();
  if(digitalRead(SDC_OUT) == 0 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(BSPD_DASH_LED) == 1 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0){
    passed = false;
    Serial.println("ERROR: Failed at BPT 2V (after reset).");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(5, 0, 0, 5, 5, 5, 5, 0, 5);

  // pass or fail
  if(passed){Serial.println("\nTests passed!");}
  else      {Serial.println("\nTests failed :(");}
}

void ctTest(){
  // set signals to DEFAULT_POSITION
  defaultPosition();
  passed = true;

  Serial.println("SDC LED should be on.");

  // ****Begin CT Testing****
  if (digitalRead(SDC_OUT) == LOW){
    passed = false; 
    Serial.println("ERROR: SDC is faulting in initial state.");
  } else {
    Serial.println("SDC is HIGH in initial state.");
  }

  Serial.println("\n****Commencing CT Unit Test****");

  //Set CT low, CT_OC_OK fault, BSPD fault
  Serial.println("Setting only CT low (CT=0V, BPT=2V)");
  setVoltage(2.0, 0);
  delay(500);
  if(digitalRead(SDC_OUT) == 1 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 1 ||
    digitalRead(BSPD_DASH_LED) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0){
    passed = false;
    Serial.println("ERROR: Failed at CT 0V.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 0, 0, 5, 5, 0, 5, 5, 5);

  delay(2000);

  //CT Low Latch Check
  Serial.println("\nChecking CT latch (CT=2V, BPT=2V)");
  setVoltage(2.0, 2.0);
  delay(500);
  if(digitalRead(SDC_OUT) == 1 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0 ||
    digitalRead(BSPD_DASH_LED) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0){
    passed = false; 
    Serial.println("ERROR: Failed to latch CT.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 0, 0, 5, 5, 5, 5, 5, 5);

  delay(2000);

  //Reset check, no faults, SDC on (default state)
  Serial.println("\nChecking reset (CT and BPT stay at 2V)");
  reset();
  if(digitalRead(SDC_OUT) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0 ||
    digitalRead(BSPD_DASH_LED) == 1 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0){
    passed = false; 
    Serial.println("ERROR: Failed at CT 2V (after reset).");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(5, 0, 0, 5, 5, 5, 5, 0, 5);

  // pass or fail
  if(passed){Serial.println("\nTests passed!");}
  else      {Serial.println("\nTests failed :(");}
}

void dualTest(){
  // set signals to DEFAULT_POSITION
  defaultPosition();
  passed = true;

  // ****Begin Dual Testing****
  Serial.println("SDC LED should be on.");
  if (digitalRead(SDC_OUT) == LOW){
    passed = false; 
    Serial.println("ERROR: SDC is faulting in initial state.");
  } else {
    Serial.println("SDC is HIGH in initial state.");
  }
  
  Serial.println("\n****Commencing Dual Unit Test****");

  // Only BPT FAULT ref, DUAL_OK high, no BSPD fault
  Serial.println("Setting only BPT FAULT Ref (BPT=3.55V, CT=2V)");
  setVoltage(3.55, 2.0);
  delay(500);
  if(digitalRead(SDC_OUT) == 0 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1){
    passed = false;
    Serial.println("ERROR: Failed at only BPT FAULT Ref.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(5, 0, 0, 5, 5, 5, 5, 0, 5);

  delay(2000);

  //Only CT High, DUAL_OK high, no BSPD fault
  Serial.println("\nSetting only CT FAULT Ref (BPT=2V, CT=2.8V)");
  setVoltage(2.0, 2.8);
  delay(500);
  if(digitalRead(SDC_OUT) == 0 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1){
    passed = false;
    Serial.println("ERROR: Failed at only CT FAULT Ref.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(5, 0, 0, 5, 5, 5, 5, 0, 5);

  delay(2000);

  //Dual Check, DUAL_OK low, BSPD fault
  Serial.println("\nChecking Dual Fault (BPT=3.55V, CT=2.8V)");
  setVoltage(3.55, 2.8);
  delay(500);
  if(digitalRead(SDC_OUT) == 1 || 
    digitalRead(DUAL_OK) == 1 || 
    digitalRead(BSPD_DASH_LED) == 0 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1){
    passed = false;
    Serial.println("ERROR: Failed at dual fault.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 0, 0, 5, 5, 5, 5, 5, 0);

  delay(2000);

  //Dual Latch Check, DUAL_OK high, still bspd fault, sdc still off
  Serial.println("\nChecking Dual Latch (BPT=2V, CT=2V)");
  setVoltage(2.0, 2.0);
  delay(500);
  if(digitalRead(SDC_OUT) == 1 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 0 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1){
    passed = false;
    Serial.println("ERROR: Failed at latching dual check.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 0, 0, 5, 5, 5, 5, 5, 5);

  delay(2000);

  //Reset Check
  Serial.println("\nChecking Reset (BPT and CT stay at 2V)");
  reset();
  if(digitalRead(SDC_OUT) == 0 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1){
    passed = false;
    Serial.println("ERROR: Failed to reset.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(5, 0, 0, 5, 5, 5, 5, 0, 5);

  // pass or fail
  if(passed){Serial.println("\nTests passed!");}
  else      {Serial.println("\nTests failed :(");}
}

// e.g. if i want to do 5 cycles: nycle(1,5)
void ncycle(int curr, int n){

  Serial.print(F("\n\n\nCYCLE "));
  Serial.print(curr);
  Serial.println(F("****************************************************************************************************************"));

  bool allPassed = true;
  // faulting bms, imd, bspd fault
  // print all outputs

  // ****Begin BMS Testing****
  defaultPosition();
  passed = true;

  Serial.println(F("SDC LED should turn on."));

  if(digitalRead(SDC_OUT) == LOW){
    Serial.println(F("ERROR: SDC is faulting in initial state."));
    passed = false;
  }
  else{
    Serial.println(F("SDC is HIGH in initial state."));
  }

  Serial.println(F("\n****Commencing BMS Unit Test****"));

  // 0V BMS, BMS fault
  Serial.println(F("Checking BMS low (BMS=0V)"));
  digitalWrite(BMS, LOW); // 0V signal
  delay(500);
  if(digitalRead(SDC_OUT) == 1 ||
    digitalRead(BMS_DASH_LED) == 0 || 
    
    digitalRead(IMD_DASH_LED) == 1 ||
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0) {
    passed = false;
    Serial.println(F("ERROR: Failed at BMS 0V."));
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 5, 0, 5, 5, 5, 5, 0, 5);

  // test latch, 24V BMS, BMS still fault, SDC still off
  Serial.println(F("\nChecking BMS latch (BMS=24V)"));
  digitalWrite(BMS, HIGH); // 24V
  delay(500);
  if(digitalRead(BMS_DASH_LED) == 0 || 
    digitalRead(SDC_OUT) == 1 ||
    
    digitalRead(IMD_DASH_LED) == 1 ||
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0) {
    passed = false;
    Serial.println(F("ERROR: Did not latch BMS fault (24V BMS but not yet reset)."));
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 5, 0, 5, 5, 5, 5, 0, 5);

  // 24V BMS, reset, BMS no fault, SDC on
  Serial.println(F("\nChecking Reset (BMS stays at 24V)"));
  reset();
  if(digitalRead(BMS_DASH_LED) == 1 || 
    digitalRead(SDC_OUT) == 0 || 
    
    digitalRead(IMD_DASH_LED) == 1 ||
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0) {
    passed = false;
    Serial.println(F("ERROR: Failed at BMS 24V."));
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(5, 0, 0, 5, 5, 5, 5, 0, 5);

  // pass or fail
  if(passed){Serial.println(F("\nBMS tests passed!"));}
  else      {Serial.println(F("\nBMS tests failed :("));}



  // ****Begin IMD Testing****
  defaultPosition();
  passed = true;

  Serial.println(F("\n\nSDC LED should turn on."));

  // autocheck if SDC is high
  if(digitalRead(SDC_OUT) == LOW){
    Serial.println(F("ERROR: SDC is faulting in initial state."));
    passed = false;
  }
  else{
    Serial.println(F("SDC is HIGH in initial state."));
  }

  Serial.println(F("\n****Commencing IMD Unit Test****"));

  // 0V IMD
  Serial.println(F("Checking IMD low (IMD=0V)"));
  digitalWrite(IMD, LOW); // 0V signal
  delay(500);
  if(digitalRead(IMD_DASH_LED) == 0 || 
    digitalRead(SDC_OUT) == 1 || 
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0) {
    passed = false;
    Serial.println(F("ERROR: Failed at IMD 0V."));
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 0, 5, 5, 5, 5, 5, 0, 5);

  // test latch, 24V IMD, IMD still fault, SDC still off
  Serial.println(F("\nChecking IMD latch (IMD=24V)"));
  digitalWrite(IMD, HIGH); // 24V
  delay(500);
  if(digitalRead(IMD_DASH_LED) == 0 || 
    digitalRead(SDC_OUT) == 1 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0) {
    passed = false;
    Serial.println(F("ERROR: Did not latch IMD fault (24V IMD but not yet reset)."));
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 0, 5, 5, 5, 5, 5, 0, 5);

  // 24V IMD, IMD no fault, SDC on
  Serial.println(F("\nChecking reset (IMD stays at 24V)"));
  reset();
  if(digitalRead(IMD_DASH_LED) == 1 || 
    digitalRead(SDC_OUT) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0) {
    passed = false;
    Serial.println(F("ERROR: Failed at IMD 24V."));
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(5, 0, 0, 5, 5, 5, 5, 0, 5);

  // pass or fail
  if(passed){Serial.println(F("\nIMD tests passed!"));}
  else      {Serial.println(F("\nIMD tests failed :("));}



  // ****Begin Dual Testing****
  defaultPosition();
  passed = true;

  // ****Begin Dual Testing****
  Serial.println("\n\nSDC LED should be on.");
  if (digitalRead(SDC_OUT) == LOW){
    passed = false; 
    Serial.println("ERROR: SDC is faulting in initial state.");
  } else {
    Serial.println("SDC is HIGH in initial state.");
  }

  Serial.println("\n****Commencing Dual Unit Test****");

  // Only BPT FAULT ref, DUAL_OK high, no BSPD fault
  Serial.println("Setting only BPT FAULT Ref (BPT=3.55V, CT=2V)");
  setVoltage(3.55, 2.0);
  delay(500);
  if(digitalRead(SDC_OUT) == 0 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1){
    passed = false;
    Serial.println("ERROR: Failed at only BPT FAULT Ref.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(5, 0, 0, 5, 5, 5, 5, 0, 5);

  //Only CT High, DUAL_OK high, no BSPD fault
  Serial.println("\nSetting only CT FAULT Ref (BPT=2V, CT=2.8V)");
  setVoltage(2.0, 2.8);
  delay(500);
  if(digitalRead(SDC_OUT) == 0 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1){
    passed = false;
    Serial.println("ERROR: Failed at only CT FAULT Ref.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(5, 0, 0, 5, 5, 5, 5, 0, 5);

  //Dual Check, DUAL_OK low, BSPD fault
  Serial.println("\nChecking Dual Fault (BPT=3.55V, CT=2.8V)");
  setVoltage(3.55, 2.8);
  delay(500);
  if(digitalRead(SDC_OUT) == 1 || 
    digitalRead(DUAL_OK) == 1 || 
    digitalRead(BSPD_DASH_LED) == 0 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1){
    passed = false;
    Serial.println("ERROR: Failed at dual fault.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 0, 0, 5, 5, 5, 5, 5, 0);

  //Dual Latch Check, DUAL_OK high, still bspd fault, sdc still off
  Serial.println("\nChecking Dual Latch (BPT=2V, CT=2V)");
  setVoltage(2.0, 2.0);
  delay(500);
  if(digitalRead(SDC_OUT) == 1 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 0 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1){
    passed = false;
    Serial.println("ERROR: Failed at latching dual check.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(0, 0, 0, 5, 5, 5, 5, 5, 5);

  //Reset Check
  Serial.println("\nChecking Reset (BPT and CT stay at 2V)");
  reset();
  if(digitalRead(SDC_OUT) == 0 || 
    digitalRead(DUAL_OK) == 0 || 
    digitalRead(BSPD_DASH_LED) == 1 ||
    digitalRead(BPT_SC_OK) == 0 ||
    digitalRead(BPT_OC_OK) == 0 ||
    digitalRead(CT_SC_OK) == 0 ||
    digitalRead(CT_OC_OK) == 0 ||
    
    digitalRead(BMS_DASH_LED) == 1 ||
    digitalRead(IMD_DASH_LED) == 1){
    passed = false;
    Serial.println("ERROR: Failed to reset.");
  }
  // sdc, bms, imd, bpt_oc, bpt_sc, ct_oc, ct_sc, bspd, dual
  printInputs(5, 0, 0, 5, 5, 5, 5, 0, 5);

  // pass or fail
  if(passed){Serial.println("\nDual tests passed!");}
  else      {Serial.println("\nDual tests failed :(");}



  // check allPassed
  if(allPassed){Serial.println("\n****ALL TESTS PASSED IN THIS CYCLE");}
  else{
    Npassed = false;
    Serial.println("\n****SOME TESTS HAVE FAILED IN THIS CYCLE");
  }

  if(curr == n){
    if(Npassed){Serial.println("\n\n****ALL N CYCLES PASSED");}
    else       {Serial.println("\n\n****SOME CYCLES HAVE FAILED");}
  }

  curr++;
  if(curr <= n){ncycle(curr, n);}
}
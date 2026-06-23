/* EXAMPLE USE CASE:
#include "DAC7612.h"

void setup()
{
  pinMode(CS, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(COPI, OUTPUT);
  pinMode(LOADDACS, OUTPUT);

  setVoltage(1.5,2.0); //BPT=1.5, CT=2.0
}

void loop(){}
*/

#include "DAC7612.h"

void shiftDAC(uint8_t A1, uint8_t A0, uint16_t data12)
{
    data12 &= 0x0FFF;   // force 12-bit

    uint16_t word = (A1 << 13) |
                    (A0 << 12) |
                    data12;

    digitalWrite(CS, LOW);
    __asm__("nop\n\t"); 

    for (int i = 13; i >= 0; i--) {
        digitalWrite(CLK, LOW);
        __asm__("nop\n\t"); 
        digitalWrite(COPI, (word >> i) & 1);
        __asm__("nop\n\t"); 
        digitalWrite(CLK, HIGH);  // rising edge shifts
        __asm__("nop\n\t"); 
    }
    __asm__("nop\n\t"); 
    digitalWrite(CS, HIGH);
}

// Load both DAC registers at once
void loadDACs()
{
    digitalWrite(LOADDACS, LOW);
    delay(1);
    digitalWrite(LOADDACS, HIGH);
}


void setVoltage(float voltageBPT, float voltageCT)
{
    if (voltageBPT < 0.0) voltageBPT = 0.0;
    if (voltageBPT > 4.095) voltageBPT = 4.095;

    if (voltageCT< 0.0) voltageCT= 0.0;
    if (voltageCT> 4.095) voltageCT= 4.095;

    uint16_t codeA = (uint16_t)(voltageBPT * 1000.0 + 0.5);
    uint16_t codeB = (uint16_t)(voltageCT* 1000.0 + 0.5);

    shiftDAC(1, 0, codeA);  // load A shift register
    loadDACs();
    shiftDAC(1, 1, codeB);  // load B shift register
    loadDACs();             // update both outputs at same time
}
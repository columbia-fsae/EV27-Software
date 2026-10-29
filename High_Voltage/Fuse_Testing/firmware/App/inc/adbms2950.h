/**
 * @file adbms2950.h
 * @brief Minimal ADBMS2950B driver over an ADBMS6822 isoSPI bridge (single device on the link)
 *
 * Command codes and register layouts are from the ADBMS2950B data sheet Rev. 0 (Table 33 command
 * list, Table 45 current results, Tables 74/75 FLAG/STAT), cross-checked against the
 * open-source Northeastern Electric Racing adbms2950 driver.
 */

#ifndef ADBMS2950_H
#define ADBMS2950_H

#include <stdbool.h>
#include <stdint.h>

// 11-bit command codes, sent as CMD0 = bits 10:8, CMD1 = bits 7:0
#define ADBMS2950_CMD_RDI     0x004  // I1ADC and I2ADC results
#define ADBMS2950_CMD_RDSTAT  0x034  // STAT register group
#define ADBMS2950_CMD_RDFLAG  0x032  // FLAG register group
#define ADBMS2950_CMD_RDSID   0x02C  // Serial ID
#define ADBMS2950_CMD_SRST    0x027  // Software reset
// ADI1 = 0 1 RD CONT 1 1 OPT2 0 x OPT1 OPT0 ; CONT (OPT[3]) is bit 7, RD is bit 8
#define ADBMS2950_CMD_ADI1    0x260
#define ADBMS2950_ADI1_CONT   (1U << 7)
#define ADBMS2950_ADI1_RD     (1U << 8)

// Every register group read returns 6 data bytes
#define ADBMS2950_REG_BYTES 6

typedef enum {
    ADBMS_OK = 0,
    ADBMS_ERR_SPI,  // HAL SPI call failed
    ADBMS_ERR_PEC,  // data PEC mismatch
} AdbmsStatus;

typedef struct {
    int32_t i1_code;   // sign-extended I1[23:0], LSB +1 uV
    int32_t i2_code;   // sign-extended I2[23:0], LSB -1 uV
} AdbmsCurrents;

// Totals since boot, reported to the PC in the stats frame
typedef struct {
    uint32_t pec_errors;
    uint32_t spi_errors;
} AdbmsCounters;

void adbms2950_wake(void);
AdbmsStatus adbms2950_send_cmd(uint16_t cmd);
AdbmsStatus adbms2950_read_reg(uint16_t cmd, uint8_t data[ADBMS2950_REG_BYTES]);

AdbmsStatus adbms2950_read_currents(AdbmsCurrents* out);
// I1CNT[10:0]: increments once per 1 ms I1ADC conversion in continuous mode
AdbmsStatus adbms2950_read_i1cnt(uint16_t* i1cnt);
AdbmsStatus adbms2950_read_i1cal(bool* done);
AdbmsStatus adbms2950_read_devid(uint8_t* devid);

const AdbmsCounters* adbms2950_counters(void);

// PEC helpers, exposed for testing
uint16_t adbms2950_pec15(const uint8_t* data, uint8_t len);
uint16_t adbms2950_pec10_rx(const uint8_t* data, uint8_t len, uint8_t cmd_counter_byte);

#endif /* ADBMS2950_H */

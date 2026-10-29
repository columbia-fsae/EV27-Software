/**
 * @file adbms2950.c
 * @brief Minimal ADBMS2950B driver over an ADBMS6822 isoSPI bridge (single device on the link)
 *
 * Frame format (same as the ADBMS6830): a command is CMD0, CMD1, PEC15[15:8], PEC15[7:0].
 * A read is followed by 6 data bytes, then a byte holding the 6-bit command counter and
 * PEC10[9:8], then PEC10[7:0].
 */

#include "adbms2950.h"

#include <string.h>

#include "app_config.h"
#include "main.h"
#include "timebase.h"

extern SPI_HandleTypeDef hspi1;

static AdbmsCounters counters;
static uint64_t last_bus_activity_us;

// ============================================================================
// PEC
// ============================================================================

uint16_t adbms2950_pec15(const uint8_t* data, uint8_t len) {
    // CRC-15, polynomial 0x4599, seed 16; the result is left-shifted so bit 0 is 0
    uint16_t rem = 16;
    for (uint8_t i = 0; i < len; i++) {
        rem ^= (uint16_t)data[i] << 7;
        for (uint8_t bit = 0; bit < 8; bit++) {
            rem = (rem & 0x4000U) ? (uint16_t)((rem << 1) ^ 0x4599U) : (uint16_t)(rem << 1);
        }
        rem &= 0x7FFFU;
    }
    return (uint16_t)(rem << 1);
}

uint16_t adbms2950_pec10_rx(const uint8_t* data, uint8_t len, uint8_t cmd_counter_byte) {
    // CRC-10, polynomial 0x08F, seed 16, over the data bytes and then the 6-bit command counter
    uint16_t rem = 16;
    for (uint8_t i = 0; i < len; i++) {
        rem ^= (uint16_t)data[i] << 2;
        for (uint8_t bit = 0; bit < 8; bit++) {
            rem = (rem & 0x200U) ? (uint16_t)((rem << 1) ^ 0x08FU) : (uint16_t)(rem << 1);
        }
    }
    rem ^= (uint16_t)(cmd_counter_byte & 0xFCU) << 2;
    for (uint8_t bit = 0; bit < 6; bit++) {
        rem = (rem & 0x200U) ? (uint16_t)((rem << 1) ^ 0x08FU) : (uint16_t)(rem << 1);
    }
    return rem & 0x3FFU;
}

// ============================================================================
// isoSPI LINK
// ============================================================================

static void cs_low(void) { HAL_GPIO_WritePin(ISOSPI_CS_PORT, ISOSPI_CS_PIN, GPIO_PIN_RESET); }
static void cs_high(void) { HAL_GPIO_WritePin(ISOSPI_CS_PORT, ISOSPI_CS_PIN, GPIO_PIN_SET); }

void adbms2950_wake(void) {
    cs_low();
    timebase_delay_us(ISOSPI_WAKE_LOW_US);
    cs_high();
    timebase_delay_us(ISOSPI_WAKE_HIGH_US);
    last_bus_activity_us = timebase_now_us();
}

static void wake_if_idle(void) {
    if ((timebase_now_us() - last_bus_activity_us) > ISOSPI_IDLE_REWAKE_US) {
        adbms2950_wake();
    }
}

static void build_cmd(uint16_t cmd, uint8_t frame[4]) {
    frame[0] = (uint8_t)((cmd >> 8) & 0x07U);
    frame[1] = (uint8_t)cmd;
    uint16_t pec = adbms2950_pec15(frame, 2);
    frame[2] = (uint8_t)(pec >> 8);
    frame[3] = (uint8_t)pec;
}

AdbmsStatus adbms2950_send_cmd(uint16_t cmd) {
    uint8_t frame[4];
    build_cmd(cmd, frame);

    wake_if_idle();
    cs_low();
    HAL_StatusTypeDef hal = HAL_SPI_Transmit(&hspi1, frame, sizeof(frame), ISOSPI_TIMEOUT_MS);
    cs_high();
    last_bus_activity_us = timebase_now_us();

    if (hal != HAL_OK) {
        counters.spi_errors++;
        return ADBMS_ERR_SPI;
    }
    return ADBMS_OK;
}

AdbmsStatus adbms2950_read_reg(uint16_t cmd, uint8_t data[ADBMS2950_REG_BYTES]) {
    // 4 command bytes out, then 6 data + 2 counter/PEC bytes back, in one CS window
    uint8_t tx[4 + ADBMS2950_REG_BYTES + 2];
    uint8_t rx[sizeof(tx)];
    memset(tx, 0xFF, sizeof(tx));
    build_cmd(cmd, tx);

    wake_if_idle();
    cs_low();
    HAL_StatusTypeDef hal = HAL_SPI_TransmitReceive(&hspi1, tx, rx, sizeof(tx), ISOSPI_TIMEOUT_MS);
    cs_high();
    last_bus_activity_us = timebase_now_us();

    if (hal != HAL_OK) {
        counters.spi_errors++;
        return ADBMS_ERR_SPI;
    }

    const uint8_t* payload = &rx[4];
    uint8_t cc_byte = payload[ADBMS2950_REG_BYTES];
    uint16_t received_pec =
        (uint16_t)(((uint16_t)(cc_byte & 0x03U) << 8) | payload[ADBMS2950_REG_BYTES + 1]);
    if (received_pec != adbms2950_pec10_rx(payload, ADBMS2950_REG_BYTES, cc_byte)) {
        counters.pec_errors++;
        return ADBMS_ERR_PEC;
    }

    memcpy(data, payload, ADBMS2950_REG_BYTES);
    return ADBMS_OK;
}

// ============================================================================
// REGISTER ACCESSORS
// ============================================================================

static int32_t sign_extend_24(const uint8_t* le) {
    uint32_t raw = (uint32_t)le[0] | ((uint32_t)le[1] << 8) | ((uint32_t)le[2] << 16);
    if (raw & 0x800000U) {
        raw |= 0xFF000000U;
    }
    return (int32_t)raw;
}

AdbmsStatus adbms2950_read_currents(AdbmsCurrents* out) {
    // RDI: bytes 0-2 = I1[23:0], bytes 3-5 = I2[23:0], little-endian two's complement
    uint8_t data[ADBMS2950_REG_BYTES];
    AdbmsStatus st = adbms2950_read_reg(ADBMS2950_CMD_RDI, data);
    if (st != ADBMS_OK) {
        return st;
    }
    out->i1_code = sign_extend_24(&data[0]);
    out->i2_code = sign_extend_24(&data[3]);
    return ADBMS_OK;
}

AdbmsStatus adbms2950_read_i1cnt(uint16_t* i1cnt) {
    // FLAG2[4:0] = I1CNT[10:6], FLAG3[7:2] = I1CNT[5:0]
    uint8_t data[ADBMS2950_REG_BYTES];
    AdbmsStatus st = adbms2950_read_reg(ADBMS2950_CMD_RDFLAG, data);
    if (st != ADBMS_OK) {
        return st;
    }
    *i1cnt = (uint16_t)(((uint16_t)(data[2] & 0x1FU) << 6) | (data[3] >> 2));
    return ADBMS_OK;
}

AdbmsStatus adbms2950_read_i1cal(bool* done) {
    // STAT1 bit 6 = I1CAL, 1 once I1ADC initialisation is complete
    uint8_t data[ADBMS2950_REG_BYTES];
    AdbmsStatus st = adbms2950_read_reg(ADBMS2950_CMD_RDSTAT, data);
    if (st != ADBMS_OK) {
        return st;
    }
    *done = (data[1] & 0x40U) != 0;
    return ADBMS_OK;
}

AdbmsStatus adbms2950_read_devid(uint8_t* devid) {
    // DEVID[5:0] = SID[46:41] = byte 5 bits 6:1
    uint8_t data[ADBMS2950_REG_BYTES];
    AdbmsStatus st = adbms2950_read_reg(ADBMS2950_CMD_RDSID, data);
    if (st != ADBMS_OK) {
        return st;
    }
    *devid = (uint8_t)((data[5] >> 1) & 0x3FU);
    return ADBMS_OK;
}

const AdbmsCounters* adbms2950_counters(void) { return &counters; }

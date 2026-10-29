/**
 * @file stream.h
 * @brief Framed binary stream to the PC over LPUART1 (ST-LINK virtual COM port)
 *
 * Every frame: 0xA5 0x5A | type u8 | len u8 | payload[len] | CRC-16/CCITT-FALSE (LE) over
 * type, len and payload. All multi-byte fields are little-endian. pc/fuse_logger.py decodes it.
 */

#ifndef STREAM_H
#define STREAM_H

#include <stdint.h>

#define STREAM_SYNC0 0xA5
#define STREAM_SYNC1 0x5A

#define STREAM_TYPE_SAMPLE 0x01
#define STREAM_TYPE_TEXT   0x02
#define STREAM_TYPE_STATS  0x03
#define STREAM_TYPE_START  0x04  // sent once each time sampling (re)starts

// Bits of StreamSample.flags
#define SAMPLE_FLAG_GAP       0x01  // I1CNT advanced by more than 1: conversions were missed
#define SAMPLE_FLAG_UNCAL     0x02  // STAT.I1CAL was not set when sampling started
#define SAMPLE_FLAG_RETRIED   0x04  // a conversion landed mid-read and the read was redone

typedef struct __attribute__((packed)) {
    uint32_t seq;        // sample frames sent since boot; a jump means frames were dropped
    uint32_t t_us;       // MCU time the new conversion was seen (wraps every ~71.6 min)
    uint16_t i1cnt;      // ADBMS2950B I1CNT[10:0] for this conversion
    int32_t i1_code;     // raw I1, +1 uV/LSB
    int32_t i2_code;     // raw I2, -1 uV/LSB (0 if ADI1_RD == 0)
    uint8_t flags;       // SAMPLE_FLAG_*
    uint8_t gap;         // I1CNT delta from the previous sample (1 when nothing was missed)
} StreamSample;

typedef struct __attribute__((packed)) {
    uint32_t t_us;
    uint32_t samples_sent;
    uint32_t conversions_missed;
    uint32_t consistency_retries;
    uint32_t pec_errors;
    uint32_t spi_errors;
    uint32_t tx_frames_dropped;
    uint32_t reinit_count;
} StreamStats;

typedef struct __attribute__((packed)) {
    uint32_t t_us;
    float shunt_ohm;
    float i1_lsb_v;
    float i2_lsb_v;
    int8_t i1_sign;
    uint8_t adi1_rd;
    uint8_t devid;
    uint8_t i1cal;
} StreamStart;

void stream_init(void);
void stream_send(uint8_t type, const void* payload, uint8_t len);
void stream_printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
uint32_t stream_dropped_frames(void);

// Returns the next received byte, or -1 if none
int stream_getc(void);

// Called from LPUART1_IRQHandler
void stream_irq_handler(void);

#endif /* STREAM_H */

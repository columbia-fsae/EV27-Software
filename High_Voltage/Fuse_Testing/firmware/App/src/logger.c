/**
 * @file logger.c
 * @brief Starts the ADBMS2950B I1ADC in continuous mode and streams every conversion to the PC
 *
 * The I1ADC produces one result every 1 ms (0.9-1.1 kS/s on the ADBMS2950B's own oscillator),
 * which is not synchronous to the MCU. Instead of reading RDI on an MCU timer (which would
 * duplicate or skip conversions as the two clocks drift), the loop polls the FLAG register's
 * I1CNT conversion counter back to back (~10 polls per ms) and reads RDI once per new
 * conversion:
 *
 *   FLAG (I1CNT = n, new)  ->  RDI  ->  FLAG (I1CNT still n?)
 *
 * If a conversion lands between the two FLAG reads, the RDI value may belong to n+1, so the
 * RDI/FLAG pair is redone. Each sample is timestamped with the MCU time at which conversion n
 * was first seen, which is within one poll (~100 us) of the conversion completing.
 */

#include "logger.h"

#include <stdbool.h>

#include "adbms2950.h"
#include "app_config.h"
#include "main.h"
#include "stream.h"
#include "timebase.h"

#define I1CNT_MASK 0x7FFU

typedef struct {
    uint32_t samples_sent;
    uint32_t conversions_missed;
    uint32_t consistency_retries;
    uint32_t reinit_count;
} LoggerCounters;

static LoggerCounters counters;

// adbms2950_* re-wakes the isoSPI link before the next transaction if it idled during a delay
static void delay_ms(uint32_t ms) { timebase_delay_us(ms * 1000U); }

// Resets the ADBMS2950B and starts I1ADC in continuous mode. Returns the I1CAL state.
static bool start_measurement(uint8_t* devid_out) {
    adbms2950_wake();
    adbms2950_send_cmd(ADBMS2950_CMD_SRST);
    delay_ms(ADBMS_SRST_SETTLE_MS);

    uint8_t devid = 0xFF;
    if (adbms2950_read_devid(&devid) != ADBMS_OK) {
        stream_printf("ERROR: RDSID failed (PEC/SPI). Check isoSPI wiring and 2950B power.");
    } else if (devid != ADBMS2950B_DEVID) {
        stream_printf("WARNING: DEVID 0x%02X, expected 0x%02X for ADBMS2950B", devid,
                      ADBMS2950B_DEVID);
    } else {
        stream_printf("ADBMS2950B found (DEVID 0x%02X)", devid);
    }
    *devid_out = devid;

    uint16_t adi1 = ADBMS2950_CMD_ADI1 | ADBMS2950_ADI1_CONT;
    if (ADI1_RD) {
        adi1 |= ADBMS2950_ADI1_RD;
    }
    adbms2950_send_cmd(adi1);
    delay_ms(ADBMS_IADC_STARTUP_MS);

    // Wait for I1ADC initialisation (tIxADC_INIT, 140 ms max) before results are in spec
    uint64_t deadline = timebase_now_us() + (uint64_t)ADBMS_I1CAL_TIMEOUT_MS * 1000U;
    bool i1cal = false;
    while (timebase_now_us() < deadline) {
        if (adbms2950_read_i1cal(&i1cal) == ADBMS_OK && i1cal) {
            break;
        }
        timebase_delay_us(1000);
    }
    if (i1cal) {
        stream_printf("I1ADC running continuously, I1CAL set");
    } else {
        stream_printf("WARNING: I1CAL not set after %d ms; samples flagged UNCAL",
                      ADBMS_I1CAL_TIMEOUT_MS);
    }
    return i1cal;
}

static void send_start_frame(uint8_t devid, bool i1cal) {
    StreamStart start = {
        .t_us = (uint32_t)timebase_now_us(),
        .shunt_ohm = (float)SHUNT_RESISTANCE_OHM,
        .i1_lsb_v = (float)I1_LSB_V,
        .i2_lsb_v = (float)I2_LSB_V,
        .i1_sign = I1_SIGN,
        .adi1_rd = ADI1_RD,
        .devid = devid,
        .i1cal = i1cal,
    };
    stream_send(STREAM_TYPE_START, &start, sizeof(start));
}

static void send_stats(void) {
    const AdbmsCounters* bus = adbms2950_counters();
    StreamStats stats = {
        .t_us = (uint32_t)timebase_now_us(),
        .samples_sent = counters.samples_sent,
        .conversions_missed = counters.conversions_missed,
        .consistency_retries = counters.consistency_retries,
        .pec_errors = bus->pec_errors,
        .spi_errors = bus->spi_errors,
        .tx_frames_dropped = stream_dropped_frames(),
        .reinit_count = counters.reinit_count,
    };
    stream_send(STREAM_TYPE_STATS, &stats, sizeof(stats));
}

// Reads RDI for conversion `cnt`, redoing it if another conversion lands mid-read.
// On success *cnt is the conversion the currents belong to.
static bool read_consistent(uint16_t* cnt, AdbmsCurrents* cur, uint8_t* flags) {
    for (uint8_t attempt = 0; attempt <= SAMPLE_MAX_CONSISTENCY_RETRIES; attempt++) {
        uint16_t cnt_after;
        if (adbms2950_read_currents(cur) != ADBMS_OK) {
            return false;
        }
        if (adbms2950_read_i1cnt(&cnt_after) != ADBMS_OK) {
            return false;
        }
        if (cnt_after == *cnt) {
            return true;
        }
        counters.consistency_retries++;
        *flags |= SAMPLE_FLAG_RETRIED;
        *cnt = cnt_after;
    }
    return false;
}

void logger_run(void) {
    uint32_t seq = 0;

    for (;;) {
        uint8_t devid;
        bool i1cal = start_measurement(&devid);
        send_start_frame(devid, i1cal);

        uint16_t last_cnt = 0;
        bool have_last = false;
        uint32_t failed_polls = 0;
        uint64_t next_stats_us = timebase_now_us() + (uint64_t)STATS_PERIOD_MS * 1000U;

        while (failed_polls < REINIT_AFTER_FAILED_POLLS) {
            // 'R' from the PC forces a reset/restart of the measurement
            if (stream_getc() == 'R') {
                stream_printf("Restart requested by PC");
                break;
            }

            uint64_t now = timebase_now_us();
            if (now >= next_stats_us) {
                send_stats();
                next_stats_us += (uint64_t)STATS_PERIOD_MS * 1000U;
            }

            uint16_t cnt;
            if (adbms2950_read_i1cnt(&cnt) != ADBMS_OK) {
                failed_polls++;
                continue;
            }
            if (have_last && cnt == last_cnt) {
                failed_polls++;
                continue;
            }
            uint64_t seen_us = timebase_now_us();

            AdbmsCurrents cur;
            uint8_t flags = i1cal ? 0 : SAMPLE_FLAG_UNCAL;
            if (!read_consistent(&cnt, &cur, &flags)) {
                failed_polls++;
                continue;
            }
            failed_polls = 0;

            uint16_t gap = have_last ? (uint16_t)((cnt - last_cnt) & I1CNT_MASK) : 1U;
            if (gap > 1U) {
                flags |= SAMPLE_FLAG_GAP;
                counters.conversions_missed += gap - 1U;
            }
            last_cnt = cnt;
            have_last = true;

            StreamSample sample = {
                .seq = seq++,
                .t_us = (uint32_t)seen_us,
                .i1cnt = cnt,
                .i1_code = cur.i1_code,
                .i2_code = cur.i2_code,
                .flags = flags,
                .gap = (uint8_t)(gap > 255U ? 255U : gap),
            };
            stream_send(STREAM_TYPE_SAMPLE, &sample, sizeof(sample));
            counters.samples_sent++;
        }

        if (failed_polls >= REINIT_AFTER_FAILED_POLLS) {
            stream_printf("ERROR: no new conversions for %lu polls, re-initialising",
                          (unsigned long)failed_polls);
        }
        counters.reinit_count++;
    }
}

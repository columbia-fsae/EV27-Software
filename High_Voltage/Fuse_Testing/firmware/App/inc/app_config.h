/**
 * @file app_config.h
 * @brief Configuration for the ADBMS2950B fuse-test current logger
 *
 * Sections:
 *   1. Wiring (NUCLEO-G474RE -> ADBMS6822 -> isoSPI -> ADBMS2950B)
 *   2. ADBMS2950B measurement setup
 *   3. Sampling loop
 *   4. PC serial link
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// ============================================================================
// 1. WIRING
// ============================================================================

// Same wiring as EV27 Tractive_Battery: SPI1 on the Arduino header
// (D13 = PA5 SCK, D12 = PA6 MISO, D11 = PA7 MOSI) and D10 = PB6 as chip select
#define ISOSPI_CS_PORT GPIOB
#define ISOSPI_CS_PIN  GPIO_PIN_6

// SPI1 runs from PCLK2 = 170 MHz; /128 gives 1.33 MHz (ADBMS limit is 2 MHz,
// and /64 = 2.66 MHz would exceed it)
#define ISOSPI_BAUD_PRESCALER SPI_BAUDRATEPRESCALER_128
#define ISOSPI_TIMEOUT_MS     5

// Wake pulse for the ADBMS6822 isoSPI port (same timing as the BMS code's adBmsWakeupIc)
#define ISOSPI_WAKE_LOW_US  1000
#define ISOSPI_WAKE_HIGH_US 1000
// Re-send the wake pulse before a transaction if the bus has been quiet this long.
// The sampling loop keeps the bus busy, so this only matters around init.
#define ISOSPI_IDLE_REWAKE_US 2000

// ============================================================================
// 2. ADBMS2950B MEASUREMENT SETUP
// ============================================================================

// Shunt on the I1 inputs (ohms)
#define SHUNT_RESISTANCE_OHM 50e-6

// I1ADC LSB is +1 uV, I2ADC LSB is -1 uV (datasheet Table 2: I2 has negative input polarity).
// Flip I1_SIGN to -1 if the shunt is wired so that the current you call positive reads negative.
#define I1_LSB_V 1e-6
#define I2_LSB_V (-1e-6)
#define I1_SIGN  (+1)

// ADI1 RD bit: 1 also starts I2ADC (and VB2ADC) synchronously with I1ADC so I2 can be logged as a
// redundant channel. Set to 0 to run I1ADC only.
#define ADI1_RD 1

// tWAKE (500 us max) + tREFUP (4.5 ms max) after SRST, rounded up
#define ADBMS_SRST_SETTLE_MS     10
// tIxADC_STARTUP (9.9 ms max) before the first result exists
#define ADBMS_IADC_STARTUP_MS    11
// tIxADC_INIT is 140 ms max; STAT.I1CAL goes high when it is done
#define ADBMS_I1CAL_TIMEOUT_MS   400

// Expected DEVID (SID[46:41]) of an ADBMS2950B. A mismatch is reported but not fatal.
#define ADBMS2950B_DEVID 0x06

// ============================================================================
// 3. SAMPLING LOOP
// ============================================================================

// The I1ADC produces one result every 1 ms (0.9-1.1 kS/s). The loop polls the FLAG register's
// conversion counter back to back and reads RDI once per new conversion, so every conversion is
// sent exactly once. These guard against a conversion landing between the reads.
#define SAMPLE_MAX_CONSISTENCY_RETRIES 3

// Re-initialise the ADBMS2950B after this many consecutive failed reads (PEC errors or no new
// conversion). At ~10 polls/ms, 2000 is ~200 ms of silence.
#define REINIT_AFTER_FAILED_POLLS 2000

// Send a statistics frame this often
#define STATS_PERIOD_MS 1000

// ============================================================================
// 4. PC SERIAL LINK
// ============================================================================

// LPUART1 on PA2/PA3 is wired to the ST-LINK V3 virtual COM port
#define PC_UART_BAUD 1000000

// Transmit ring buffer. One sample frame is 26 bytes, so this holds ~150 ms of samples.
#define PC_TX_BUFFER_SIZE 4096

#endif /* APP_CONFIG_H */

/**
 * @file kalman_soc_config.h
 * @brief Configuration with R/C lookup tables and flash support
 */

#ifndef KALMAN_SOC_CONFIG_H
#define KALMAN_SOC_CONFIG_H

// ============================================================================
// BATTERY PACK CONFIGURATION
// ============================================================================

#define NUM_SERIES_CELLS        TOTAL_MODULES*CELLS_PER_MOD
#define NUM_PARALLEL_STRINGS    1
#define NOMINAL_CELL_CAPACITY_AH 12.0f    // 3600mAh cells
#define PACK_CAPACITY_AS        (NUM_SERIES_CELLS * NUM_PARALLEL_STRINGS * \
                                 NOMINAL_CELL_CAPACITY_AH * 3.6f)
#define MAX_CELL_VOLTAGE        4.2f
#define MIN_CELL_VOLTAGE        2.5f

// ============================================================================
// R/C LOOKUP TABLE SIZES
// ============================================================================

// Grid sizes for 2D lookups (SOC × Temperature)
#define RC_LUT_SOC_SIZE         10   // 10 SOC points (0.1 to 1.0)
#define RC_LUT_TEMP_SIZE        2   // 5 temp points (e.g., 0°C, 10°C, 20°C, 30°C, 40°C)

// ============================================================================
// KALMAN FILTER TUNING
// ============================================================================

// Initial parameter values
#define INITIAL_R0              0.005f
#define INITIAL_QNOM            (NOMINAL_CELL_CAPACITY_AH * 3600.0f)

// Process noise (from MATLAB)
#define Q_SOC                   (1e-7f)//(1000.0f * 1e-4f)
#define Q_VCT                   (1e-4f)//(0.1f * 1e-4f)
#define Q_VDIF                  (1e-5f)//(0.01f * 1e-4f)

// Measurement noise (from MATLAB)
#define R_VOLTAGE               1e-3f
#define R_SOC_PSEUDO            1e-1f

// Parameter estimation noise (from MATLAB)
#define Q_R0                    1e-11f
#define Q_QNOM                  1e-14f

#define R_PARAM_R0              1.0f
#define R_PARAM_QNOM            1000.0f

// Innovation window for adaptive covariance
#define INNOVATION_WINDOW_SIZE  100

// Update rate
#define SOC_UPDATE_PERIOD_MS    100  // 10 Hz


#define MAX_ALLOWED_DI_DT 40.0f // Amps per second threshold (adjust to your car)
#define MAX_PHYSICAL_INNOVATION_V 0.250f // 250 mV gate limit

// ============================================================================
// FLASH STORAGE CONFIGURATION
// ============================================================================

// Flash sector for SOC data storage (STM32G4 specific)
// Bank 2, Page 127 (last page) - 2KB
#define FLASH_SOC_STORAGE_PAGE      127
#define FLASH_SOC_STORAGE_ADDRESS   0x0803F800   // Bank 2, last page

// How often to write to flash (every N successful updates)
#define FLASH_WRITE_INTERVAL        100   // Every 10 seconds at 10Hz

// ============================================================================
// SOH CALCULATION
// ============================================================================

// SOC threshold for SOH calculation (only update at high SOC)
#define SOH_UPDATE_SOC_THRESHOLD    0.9f   // 90% SOC

#endif

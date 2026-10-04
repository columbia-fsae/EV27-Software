/**
 * @file kalman_soc_config.h
 * @brief Configuration with R/C lookup tables and flash support
 */

#ifndef KALMAN_SOC_CONFIG_H
#define KALMAN_SOC_CONFIG_H

#include "adbms_config.h"

// ============================================================================
// BATTERY PACK CONFIGURATION
// ============================================================================

#define NUM_SERIES_CELLS TOTAL_MODULES* CELLS_PER_MOD
#define NUM_PARALLEL_STRINGS 1
#define NOMINAL_CELL_CAPACITY_AH 12.0f  // 3600mAh cells
#define PACK_CAPACITY_AS (NUM_SERIES_CELLS * NUM_PARALLEL_STRINGS * NOMINAL_CELL_CAPACITY_AH * 3.6f)
#define MAX_CELL_VOLTAGE 4.2f
#define MIN_CELL_VOLTAGE 2.5f

// ============================================================================
// R/C LOOKUP TABLE SIZES
// ============================================================================

// Grid sizes for 2D lookups (SOC × Temperature)
#define RC_LUT_SOC_SIZE 10  // 10 SOC points (0.1 to 1.0)
#define RC_LUT_TEMP_SIZE 2  // 5 temp points (e.g., 0°C, 10°C, 20°C, 30°C, 40°C)

// ============================================================================
// KALMAN FILTER TUNING
// ============================================================================

// Initial parameter values
#define INITIAL_R0 0.005f
#define INITIAL_QNOM (NOMINAL_CELL_CAPACITY_AH * 3600.0f)

// Process noise (from MATLAB)
#define Q_SOC (1e-7f)   //(1000.0f * 1e-4f)
#define Q_VCT (1e-4f)   //(0.1f * 1e-4f)
#define Q_VDIF (1e-5f)  //(0.01f * 1e-4f)

// Measurement noise (from MATLAB)
#define R_VOLTAGE 1e-3f
#define R_SOC_PSEUDO 1e-1f

// Parameter estimation noise (from MATLAB)
#define Q_R0 1e-11f
#define Q_QNOM 1e-14f

#define R_PARAM_R0 1.0f
#define R_PARAM_QNOM 1000.0f

// Innovation window for adaptive covariance
#define INNOVATION_WINDOW_SIZE 100

// Update rate
#define SOC_UPDATE_PERIOD_MS 100  // 10 Hz

#define MAX_ALLOWED_DI_DT 40.0f           // Amps per second threshold (adjust to your car)
#define MAX_PHYSICAL_INNOVATION_V 0.250f  // 250 mV gate limit

// ============================================================================
// FLASH STORAGE CONFIGURATION
// ============================================================================

// Flash sector for SOC data storage (STM32G4 specific)
// Bank 1, Page 127 (last page) - 2KB
#define FLASH_SOC_STORAGE_PAGE 127
#define FLASH_SOC_STORAGE_ADDRESS 0x0803F800UL  // Bank 1, last page
#define FLASH_SOC_STORAGE_BANK FLASH_BANK_1

// How often to write to flash (every N successful updates)
#define FLASH_WRITE_INTERVAL 100  // Every 10 seconds at 10Hz

// ============================================================================
// SOH CALCULATION
// ============================================================================

// SOC threshold for SOH calculation (only update at high SOC)
#define SOH_UPDATE_SOC_THRESHOLD 0.9f  // 90% SOC

// ============================================================================
// FILTER INITIAL CONDITIONS
// ============================================================================

// SOC used when the caller passes a negative initial SOC
#define KALMAN_DEFAULT_INITIAL_SOC 0.3f
// Initial measurement-model state
#define KALMAN_INITIAL_Z 0.3f
// SOH assumed at start-up (1.0 = 100%)
#define KALMAN_INITIAL_SOH 1.0f

// Initial state covariance P (diagonal entries)
#define KALMAN_INITIAL_P_DIAG 1e-3f
// Initial parameter covariance P_theta (R0, Qnom)
#define KALMAN_INITIAL_P_THETA_R0 0.1f
#define KALMAN_INITIAL_P_THETA_QNOM 0.001f

// R/C values used when the lookup tables have not been set
#define FALLBACK_R_CT 0.003f
#define FALLBACK_C_CT 1000.0f
#define FALLBACK_R_DIF 0.006f
#define FALLBACK_C_DIF 100000.0f

// ============================================================================
// FILTER NUMERICAL LIMITS
// ============================================================================

// Minimum time step (s) for computing the current derivative
#define KALMAN_MIN_DT_S 1e-5f
// Innovation covariance determinants below this are treated as singular
#define KALMAN_MIN_DETERMINANT 1e-10f

// Upper ceilings on adaptive process noise added to Q_SOC / Q_VCT / Q_VDIF
#define Q_ADAPTIVE_MAX_SOC 1e-5f
#define Q_ADAPTIVE_MAX_VCT 1e-3f
#define Q_ADAPTIVE_MAX_VDIF 1e-4f

// ============================================================================
// SOH LIMITS
// ============================================================================

// SOH estimate is clamped to this range (1.0 = 100%)
#define SOH_MIN 0.5f
#define SOH_MAX 1.5f

// ============================================================================
// SOC LOOP AND PERSISTENCE
// ============================================================================

// Starting value when searching for the lowest cell SOC (any real SOC is below it)
#define SOC_MIN_SEARCH_INIT 2
// Interval between periodic SOC saves to flash
#define SOC_FLASH_SAVE_INTERVAL_MS 300000  // 5 minutes

// Value stamped on saved state so a blank flash page is not mistaken for data
#define KALMAN_MAGIC_NUMBER 0xDEADBEEF

// Sanity limits applied when validating state loaded from flash
#define PERSIST_SOC_MIN 0.0f
#define PERSIST_SOC_MAX 1.0f
#define PERSIST_R0_MIN 0.0f
#define PERSIST_R0_MAX 1.0f
// Qnom must be within these factors of INITIAL_QNOM
#define PERSIST_QNOM_MIN_FACTOR 0.5f
#define PERSIST_QNOM_MAX_FACTOR 2.0f

// ============================================================================
// BATTERY MODEL (SOC-OCV LOOKUP)
// ============================================================================

// Number of points in lookup table
#define OCV_LUT_SIZE 11

// Points are {SOC, OCV (V), slope dV/dSOC}
// Downsampled from original 4964-point table; a typical Li-ion cell OCV curve
#define OCV_LUT_DATA                                                                           \
    {{0.00f, 2.780000f, 5.31345f}, {0.10f, 3.206929f, 2.88707f}, {0.20f, 3.425026f, 1.28224f}, \
     {0.30f, 3.515832f, 0.98664f}, {0.40f, 3.623843f, 0.93854f}, {0.50f, 3.706822f, 0.79836f}, \
     {0.60f, 3.783746f, 0.80725f}, {0.70f, 3.868669f, 0.91243f}, {0.80f, 3.967249f, 0.96229f}, \
     {0.90f, 4.061237f, 0.91914f}, {1.00f, 4.151168f, 0.87902f}}

// Interpolation spans narrower than this are treated as zero width
#define INTERP_MIN_SPAN 1e-8f

// Table integrity checks (BatteryModel_VerifyTable)
// OCV may fall by at most this much between neighbouring points...
#define OCV_MAX_DROP_V 0.1f
// ...except in the top points, where the curve is allowed to turn over
#define OCV_MONOTONIC_EXCLUDE_TOP_POINTS 5
// Every OCV point must lie within this range (V)
#define OCV_VALID_MIN_V 2.0f
#define OCV_VALID_MAX_V 5.0f

// ============================================================================
// R/C LOOKUP TABLE DATA (from HPPC testing)
// ============================================================================

// Grid shared by every table: temperature (degC) and SOC (0-1)
#define RC_LUT_TEMP_POINTS {25.0f, 26.0f}
#define RC_LUT_SOC_POINTS {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.0f}

#define RC_LUT_R_CT_VALUES                                                    \
    {{0.0007293401782525250f, 0.0009074875325995350f, 0.00085638685177882f,   \
      0.0012423409779717100f, 0.0011932057340083800f, 0.001315810325699840f,  \
      0.0008616053304370760f, 0.0012148334716370600f, 0.0023088303124354100f, \
      0.004822437902900810f},                                                 \
     {0.0007293401782525250f, 0.0009074875325995350f, 0.00085638685177882f,   \
      0.0012423409779717100f, 0.0011932057340083800f, 0.001315810325699840f,  \
      0.0008616053304370760f, 0.0012148334716370600f, 0.0023088303124354100f, \
      0.004822437902900810f}}

#define RC_LUT_C_CT_VALUES                                                              \
    {{3426.8040222159100f, 4055.2212079149300f, 4828.111556415460f, 5634.523954468950f, \
      5866.549079080130f, 5319.9156924664600f, 7077.136410907460f, 5762.106628957510f,  \
      3031.838226611050f, 1451.5479806985900f},                                         \
     {3426.8040222159100f, 4055.2212079149300f, 4828.111556415460f, 5634.523954468950f, \
      5866.549079080130f, 5319.9156924664600f, 7077.136410907460f, 5762.106628957510f,  \
      3031.838226611050f, 1451.5479806985900f}}

#define RC_LUT_R_DIF_VALUES                                                                        \
    {{0.0018522507006706300f, 0.0023720948484197800f, 0.002067557002357570f, 0.00125871530333011f, \
      0.0014997750862781800f, 0.001823055274690330f, 0.001862907879059510f, 0.00126993563273167f,  \
      0.002673415576968640f, 0.004061331586355210f},                                               \
     {0.0018522507006706300f, 0.0023720948484197800f, 0.002067557002357570f, 0.00125871530333011f, \
      0.0014997750862781800f, 0.001823055274690330f, 0.001862907879059510f, 0.00126993563273167f,  \
      0.002673415576968640f, 0.004061331586355210f}}

#define RC_LUT_C_DIF_VALUES                                                              \
    {{10829.999076136800f, 14390.588433361300f, 17705.78890692150f, 43171.568400062000f, \
      26935.39645917410f, 32112.190637379200f, 22994.811807973600f, 31368.700780914500f, \
      21714.92126684060f, 12507.460992043400f},                                          \
     {10829.999076136800f, 14390.588433361300f, 17705.78890692150f, 43171.568400062000f, \
      26935.39645917410f, 32112.190637379200f, 22994.811807973600f, 31368.700780914500f, \
      21714.92126684060f, 12507.460992043400f}}

#define RC_LUT_R0_VALUES                                                                          \
    {{0.005309061985376070f, 0.005045924714300260f, 0.004985194918799730f, 0.005032740842521390f, \
      0.005065978524900830f, 0.00499394960654918f, 0.005033886178286290f, 0.005035637273081450f,  \
      0.005094409985592770f, 0.0060653011513259000f},                                             \
     {0.005309061985376070f, 0.005045924714300260f, 0.004985194918799730f, 0.005032740842521390f, \
      0.005065978524900830f, 0.00499394960654918f, 0.005033886178286290f, 0.005035637273081450f,  \
      0.005094409985592770f, 0.0060653011513259000f}}

#endif  // KALMAN_SOC_CONFIG_H

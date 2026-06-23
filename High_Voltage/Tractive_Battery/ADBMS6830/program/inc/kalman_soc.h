/**
 * @file kalman_soc.h  
 * @brief EXACT MATLAB port with R/C lookups, SOH, and flash persistence
 */

#ifndef KALMAN_SOC_H
#define KALMAN_SOC_H

#include <stdint.h>
#include <stdbool.h>
#include "kalman_soc_config.h"
#include "common_types.h"

// ============================================================================
// R/C LOOKUP TABLE STRUCTURES
// ============================================================================

// 2D lookup table for R_ct, C_ct, R_dif, C_dif as function of (SOC, Temperature)
typedef struct {
    float soc_points[RC_LUT_SOC_SIZE];      // SOC grid points
    float temp_points[RC_LUT_TEMP_SIZE];    // Temperature grid points
    float values[RC_LUT_TEMP_SIZE][RC_LUT_SOC_SIZE];  // Lookup values
} RC_LookupTable;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

typedef struct {
    float soc;
    float v_ct;
    float v_dif;
} StateVector;

typedef struct {
    float r0;
    float q_nom;
} ParameterVector;

typedef struct {
    float data[3][3];
} CovMatrix3x3;

typedef struct {
    float data[2][2];
} CovMatrix2x2;

// ============================================================================
// FLASH PERSISTENCE STRUCTURE
// ============================================================================

typedef struct {
    // State vector
    float soc;
    float v_ct;
    float v_dif;
    
    // Parameters
    float r0;
    float q_nom;
    
    // Covariances (optional - can omit to save space)
    float P[3][3];
    float P_theta[2][2];
    
    // SOH
    float soh_est;
    
    // Validation
    uint32_t magic_number;  // 0xDEADBEEF to verify valid data
    uint32_t crc32;         // CRC32 checksum
    
} KalmanSOC_PersistentState;

#define KALMAN_MAGIC_NUMBER 0xDEADBEEF

// ============================================================================
// MAIN FILTER STRUCTURE
// ============================================================================

typedef struct {
    // MATLAB: obj.x
    StateVector x;
    
    // MATLAB: obj.theta
    ParameterVector theta;
    
    // MATLAB: obj.p
    CovMatrix3x3 P;
    
    // MATLAB: obj.p_theta
    CovMatrix2x2 P_theta;
    
    // MATLAB: obj.k
    float k[3][2];
    
    // MATLAB: obj.k_theta
    float k_theta[2][2];
    
    // MATLAB: obj.c_theta
    float c_theta[2][2];
    
    // MATLAB: obj.dx_dtheta
    float dx_dtheta[3][2];
    
    // MATLAB: obj.q_fixed (now persistent)
    float q_fixed[3][3];
    
    // MATLAB: obj.d_window
    float innovation_window[INNOVATION_WINDOW_SIZE];
    uint16_t window_index;
    
    // MATLAB: obj.I_prev
    float i_prev;
    
    // MATLAB: obj.q_prev
    float q_prev;
    
    // MATLAB: obj.t
    float t;
    
    // MATLAB: obj.z
    float z;
    
    // NEW: SOH estimation
    float soh_est;
    
    // NEW: R/C lookup tables (pointers to avoid large stack usage)
    const RC_LookupTable *r_ct_lut;
    const RC_LookupTable *c_ct_lut;
    const RC_LookupTable *r_dif_lut;
    const RC_LookupTable *c_dif_lut;
    const RC_LookupTable *g_r0_lut;
    
    bool initialized;
    
} KalmanSOC;

typedef struct {
    float voltage;
    float current;
    float temperature;
    uint32_t timestamp_ms;
} SOC_Measurement;


//extern volatile SOC_Estimate g_soc_estimate;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

/**
 * @brief Initialize Kalman filter from scratch
 */
void KalmanSOC_Init(KalmanSOC *kf, float initial_soc);

/**
 * @brief Initialize from rest voltage
 */
void KalmanSOC_InitFromVoltage(KalmanSOC *kf, float rest_voltage, float temp_c);

/**
 * @brief Initialize from flash-stored persistent state
 * @param kf Filter structure
 * @param persistent Persistent state loaded from flash
 * @return true if successful, false if data invalid
 */
bool KalmanSOC_InitFromFlash(KalmanSOC *kf, const KalmanSOC_PersistentState *persistent);

/**
 * @brief Update SOC estimate (main algorithm)
 */
bool KalmanSOC_Update(KalmanSOC *kf, const SOC_Measurement *meas, SOC_Estimate *estimate);

/**
 * @brief Set R/C lookup tables
 * @details Must be called after Init() to enable temperature compensation
 */
void KalmanSOC_SetRCLookupTables(KalmanSOC *kf,
                                  const RC_LookupTable *r_ct,
                                  const RC_LookupTable *c_ct,
                                  const RC_LookupTable *r_dif,
                                  const RC_LookupTable *c_dif,
								  const RC_LookupTable *r_0);

/**
 * @brief Set R0 vs SOC lookup table for SOH calculation
 */
void KalmanSOC_SetR0Lookup(KalmanSOC *kf,
                           const float *soc_points,
                           const float *r0_values,
                           uint16_t size);

/**
 * @brief Get current state for flash storage
 * @param kf Filter structure
 * @param persistent Output persistent state structure
 */
void KalmanSOC_GetPersistentState(const KalmanSOC *kf, KalmanSOC_PersistentState *persistent);

/**
 * @brief Calculate CRC32 for persistent state
 */
uint32_t KalmanSOC_CalculateCRC32(const KalmanSOC_PersistentState *persistent);

/**
 * @brief Validate persistent state from flash
 */
bool KalmanSOC_ValidatePersistentState(const KalmanSOC_PersistentState *persistent);

void KalmanSOC_Reset(KalmanSOC *kf);
void KalmanSOC_GetEstimate(const KalmanSOC *kf, SOC_Estimate *estimate);
bool KalmanSOC_IsHealthy(const KalmanSOC *kf);
void KalmanSOC_GetStats(const KalmanSOC *kf, float *max_time_us, float *avg_time_us);

#endif

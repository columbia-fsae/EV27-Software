/**
 * @file battery_model.h
 * @brief Battery model with OCV-SOC relationship
 * @details Provides lookup tables and battery parameter functions
 * 
 * @author BMS Integration
 * @date 2026
 */

#ifndef BATTERY_MODEL_H
#define BATTERY_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include "kalman_soc_config.h"

// ============================================================================
// SOC-OCV LOOKUP TABLE (50 points - optimized from original 4964)
// ============================================================================

// Number of points in lookup table
#define OCV_LUT_SIZE 11

// Lookup table structure
typedef struct {
    float soc;      // State of charge (0.0 to 1.0)
    float ocv;      // Open circuit voltage (V)
    float slope;    // dV/dSOC gradient
} OcvLutPoint;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

/**
 * @brief Initialize battery model
 * @details Prepares lookup tables and parameters
 */
void BatteryModel_Init(void);

/**
 * @brief Get OCV for given SOC
 * @param soc State of charge (0.0 to 1.0)
 * @return Open circuit voltage in volts
 */
float BatteryModel_GetOcv(float soc);

/**
 * @brief Get SOC from OCV (inverse lookup)
 * @param ocv Open circuit voltage in volts
 * @return State of charge (0.0 to 1.0)
 */
float BatteryModel_GetSocFromOcv(float ocv);

/**
 * @brief Get dV/dSOC slope at given SOC
 * @param soc State of charge (0.0 to 1.0)
 * @return Voltage gradient (V per SOC unit)
 */
float BatteryModel_GetSlope(float soc);

/**
 * @brief Get temperature-compensated resistance
 * @param r_base Base resistance at 25°C
 * @param temp_c Temperature in Celsius
 * @return Temperature-compensated resistance
 */
float BatteryModel_GetResistance(float r_base, float temp_c);

/**
 * @brief Get RC time constant
 * @param r Resistance (Ohm)
 * @param c Capacitance (F)
 * @return Time constant tau (seconds)
 */
static inline float BatteryModel_GetTau(float r, float c) {
    return r * c;
}

// ============================================================================
// UTILITY FUNCTIONS (for debugging/calibration)
// ============================================================================

/**
 * @brief Get nearest OCV table point index
 * @param soc State of charge
 * @return Index in lookup table
 */
int BatteryModel_GetNearestIndex(float soc);

/**
 * @brief Get table point by index
 * @param index Table index
 * @param soc Output SOC value (can be NULL)
 * @param ocv Output OCV value (can be NULL)
 * @param slope Output slope value (can be NULL)
 * @return true if successful, false if index out of range
 */
bool BatteryModel_GetTablePoint(int index, float *soc, float *ocv, float *slope);

/**
 * @brief Verify lookup table integrity
 * @return true if table is valid, false if corrupted
 */
bool BatteryModel_VerifyTable(void);

#endif // BATTERY_MODEL_H

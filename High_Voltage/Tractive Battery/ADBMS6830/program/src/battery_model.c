/**
 * @file battery_model.cpp
 * @brief Battery model implementation with OCV-SOC lookup
 * @details Provides SOC-OCV relationship and battery parameters
 *          Optimized 50-point lookup table (reduced from 4964 points)
 * 
 * @author Adapted for ADBMS6830 BMS
 * @date 2026
 */

#include "battery_model.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>

// ============================================================================
// LOOKUP TABLE DATA (50 points - Li-ion cell typical)
// ============================================================================

// Downsam pled from original 4964-point table
// This represents a typical Li-ion cell OCV curve
static const OcvLutPoint g_ocv_lut[OCV_LUT_SIZE] = {
    // SOC,   OCV(V),  Slope(V/SOC)
	{0.00f, 2.780000f, 5.31345f},
    {0.10f, 3.206929f, 2.88707f},
	{0.20f, 3.425026f, 1.28224f},
	{0.30f, 3.515832f, 0.98664f},
	{0.40f, 3.623843f, 0.93854f},
	{0.50f, 3.706822f, 0.79836f},
	{0.60f, 3.783746f, 0.80725f},
	{0.70f, 3.868669f, 0.91243f},
	{0.80f, 3.967249f, 0.96229f},
	{0.90f, 4.061237f, 0.91914f},
	{1.00f, 4.151168f, 0.87902f}
};

// ============================================================================
// PRIVATE FUNCTIONS
// ============================================================================

/**
 * @brief Binary search in lookup table
 * @param value Value to search for in the table
 * @param is_soc true if searching by SOC, false if searching by OCV
 * @return Index of lower bound in table
 */
static int binary_search_lut(float value, bool is_soc) {
    int low = 0;
    int high = OCV_LUT_SIZE - 1;
    
    // Handle boundary cases
    if (is_soc) {
        if (value <= g_ocv_lut[0].soc) return 0;
        if (value >= g_ocv_lut[high].soc) return high - 1;
    } else {
        if (value <= g_ocv_lut[0].ocv) return 0;
        if (value >= g_ocv_lut[high].ocv) return high - 1;
    }
    
    // Binary search
    while (low < high - 1) {
        int mid = (low + high) / 2;
        float mid_val = is_soc ? g_ocv_lut[mid].soc : g_ocv_lut[mid].ocv;
        
        if (value < mid_val) {
            high = mid;
        } else {
            low = mid;
        }
    }
    
    return low;
}

/**
 * @brief Linear interpolation between two points
 */
static float interpolate(float x, float x0, float x1, float y0, float y1) {
    if (fabsf(x1 - x0) < 1e-8f) {
        return y0;  // Avoid division by zero
    }
    
    float t = (x - x0) / (x1 - x0);
    return y0 + t * (y1 - y0);
}

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

void BatteryModel_Init(void) {
    // Nothing to initialize for now
    // Could add runtime parameter loading here
}

float BatteryModel_GetOcv(float soc) {
    // Clamp SOC to valid range
    if (soc <= 0.0f) return g_ocv_lut[0].ocv;
    if (soc >= 1.0f) return g_ocv_lut[OCV_LUT_SIZE - 1].ocv;
    
    // Find interpolation indices
    int idx = binary_search_lut(soc, true);
    
    // Linear interpolation
    float soc0 = g_ocv_lut[idx].soc;
    float soc1 = g_ocv_lut[idx + 1].soc;
    float ocv0 = g_ocv_lut[idx].ocv;
    float ocv1 = g_ocv_lut[idx + 1].ocv;
    
    return interpolate(soc, soc0, soc1, ocv0, ocv1);
}

float BatteryModel_GetSocFromOcv(float ocv) {
    // Clamp OCV to valid range
    if (ocv <= g_ocv_lut[0].ocv) return 0.0f;
    
    // Handle special case at high SOC where voltage drops
    if (ocv >= g_ocv_lut[OCV_LUT_SIZE - 2].ocv) {
        // Linear interpolation in the last segment
        int idx = OCV_LUT_SIZE - 2;
        float ocv0 = g_ocv_lut[idx].ocv;
        float ocv1 = g_ocv_lut[idx + 1].ocv;
        float soc0 = g_ocv_lut[idx].soc;
        float soc1 = g_ocv_lut[idx + 1].soc;
        
        return interpolate(ocv, ocv0, ocv1, soc0, soc1);
    }
    
    // Find interpolation indices
    int idx = binary_search_lut(ocv, false);
    
    // Linear interpolation
    float ocv0 = g_ocv_lut[idx].ocv;
    float ocv1 = g_ocv_lut[idx + 1].ocv;
    float soc0 = g_ocv_lut[idx].soc;
    float soc1 = g_ocv_lut[idx + 1].soc;
    
    float soc = interpolate(ocv, ocv0, ocv1, soc0, soc1);
    
    // Clamp result
    if (soc < 0.0f) return 0.0f;
    if (soc > 1.0f) return 1.0f;
    
    return soc;
}

float BatteryModel_GetSlope(float soc) {
    // Clamp SOC to valid range
    if (soc <= 0.0f) return g_ocv_lut[0].slope;
    if (soc >= 1.0f) return g_ocv_lut[OCV_LUT_SIZE - 1].slope;
    
    // Find interpolation indices
    int idx = binary_search_lut(soc, true);
    
    // Linear interpolation
    float soc0 = g_ocv_lut[idx].soc;
    float soc1 = g_ocv_lut[idx + 1].soc;
    float slope0 = g_ocv_lut[idx].slope;
    float slope1 = g_ocv_lut[idx + 1].slope;
    
    return interpolate(soc, soc0, soc1, slope0, slope1);
}

float BatteryModel_GetResistance(float r_base, float temp_c) {
    // Temperature compensation (Arrhenius-like)
    // For now, just return base value
    // TODO: Implement temperature dependency
    // R(T) = R_25 * exp(alpha * (1/T - 1/298))
    
    (void)temp_c;  // Unused for now
    return r_base;
}

// ============================================================================
// UTILITY FUNCTIONS FOR CALIBRATION
// ============================================================================

/**
 * @brief Get nearest OCV table point (for debugging)
 */
int BatteryModel_GetNearestIndex(float soc) {
    if (soc <= 0.0f) return 0;
    if (soc >= 1.0f) return OCV_LUT_SIZE - 1;
    return binary_search_lut(soc, true);
}

/**
 * @brief Get table point by index (for debugging/calibration)
 */
bool BatteryModel_GetTablePoint(int index, float *soc, float *ocv, float *slope) {
    if (index < 0 || index >= OCV_LUT_SIZE) {
        return false;
    }
    
    if (soc != NULL) *soc = g_ocv_lut[index].soc;
    if (ocv != NULL) *ocv = g_ocv_lut[index].ocv;
    if (slope != NULL) *slope = g_ocv_lut[index].slope;
    
    return true;
}

/**
 * @brief Verify lookup table integrity
 * @return true if table is valid, false if corrupted
 */
bool BatteryModel_VerifyTable(void) {
    // Check SOC is monotonically increasing
    for (int i = 0; i < OCV_LUT_SIZE - 1; i++) {
        if (g_ocv_lut[i].soc >= g_ocv_lut[i + 1].soc) {
            return false;  // SOC not increasing
        }
    }
    
    // Check OCV is generally increasing (except at high SOC)
    for (int i = 0; i < OCV_LUT_SIZE - 5; i++) {
        if (g_ocv_lut[i].ocv > g_ocv_lut[i + 1].ocv + 0.1f) {
            return false;  // Large voltage drop unexpected
        }
    }
    
    // Check SOC bounds
   // if (g_ocv_lut[0].soc != 0.0f || g_ocv_lut[OCV_LUT_SIZE - 1].soc != 1.0f) {
    //    return false;
   // }
    
    // Check voltage is in reasonable range
    for (int i = 0; i < OCV_LUT_SIZE; i++) {
        if (g_ocv_lut[i].ocv < 2.0f || g_ocv_lut[i].ocv > 5.0f) {
            return false;
        }
    }
    
    return true;
}

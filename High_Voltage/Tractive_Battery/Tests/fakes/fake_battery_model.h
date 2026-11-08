/**
 * @file fake_battery_model.h
 * @brief Use when a test needs to pin the OCV curve (e.g. a flat or step OCV for
 * kalman_soc edge cases); prefer the real battery_model.c otherwise.
 *
 * FFF fakes for every function in battery_model.h (BatteryModel_GetTau is
 * static inline and stays real). battery_model.c owns no global variables.
 * Add `FAKES battery_model` to add_fw_test() to use it.
 *
 * custom_fake needed:
 *   - BatteryModel_GetOcv / GetSocFromOcv / GetSlope: return_val is one
 *     constant; use a custom_fake for a curve.
 *   - BatteryModel_GetTablePoint fills its soc/ocv/slope output pointers only
 *     in a custom_fake.
 */
#ifndef FAKE_BATTERY_MODEL_H
#define FAKE_BATTERY_MODEL_H

#include "battery_model.h"
#include "fff.h"

// clang-format off
#define FAKE_BATTERY_MODEL_LIST(VALUE, VOID)                             \
    VOID(BatteryModel_Init)                                              \
    VALUE(float, BatteryModel_GetOcv, float)                             \
    VALUE(float, BatteryModel_GetSocFromOcv, float)                      \
    VALUE(float, BatteryModel_GetSlope, float)                           \
    VALUE(float, BatteryModel_GetResistance, float, float)               \
    VALUE(int, BatteryModel_GetNearestIndex, float)                      \
    VALUE(bool, BatteryModel_GetTablePoint, int, float*, float*, float*) \
    VALUE(bool, BatteryModel_VerifyTable)
// clang-format on

FAKE_BATTERY_MODEL_LIST(DECLARE_FAKE_VALUE_FUNC, DECLARE_FAKE_VOID_FUNC)

// Reset every fake above.
void fake_battery_model_reset(void);

#endif

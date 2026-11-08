/**
 * @file fake_kalman_soc.h
 * @brief Use when testing SOC plumbing (adBms_Application's soc_* functions,
 * soc_flash_storage) without running the real Kalman filter.
 *
 * FFF fakes for the functions in kalman_soc.h that kalman_soc.c defines.
 * KalmanSOC_SetR0Lookup is declared in the header but defined nowhere, so it
 * is not faked. kalman_soc.c owns no global variables. Add `FAKES kalman_soc`
 * to add_fw_test() to use it.
 *
 * custom_fake needed (output pointers are only filled in a custom_fake):
 *   - KalmanSOC_Update (estimate), KalmanSOC_GetEstimate (estimate)
 *   - KalmanSOC_GetPersistentState (persistent)
 *   - KalmanSOC_GetStats (max_time_us, avg_time_us)
 *   - KalmanSOC_Init* only touch *kf in a custom_fake; set kf->initialized
 *     there if the code under test checks it.
 */
#ifndef FAKE_KALMAN_SOC_H
#define FAKE_KALMAN_SOC_H

#include "fff.h"
#include "kalman_soc.h"

// clang-format off
#define FAKE_KALMAN_SOC_LIST(VALUE, VOID)                                              \
    VOID(KalmanSOC_Init, KalmanSOC*, float)                                            \
    VOID(KalmanSOC_InitFromVoltage, KalmanSOC*, float, float)                          \
    VALUE(bool, KalmanSOC_InitFromFlash, KalmanSOC*, const KalmanSOC_PersistentState*) \
    VALUE(bool, KalmanSOC_Update, KalmanSOC*, const SOC_Measurement*, SOC_Estimate*)   \
    VOID(KalmanSOC_SetRCLookupTables, KalmanSOC*, const RC_LookupTable*,               \
         const RC_LookupTable*, const RC_LookupTable*, const RC_LookupTable*,          \
         const RC_LookupTable*)                                                        \
    VOID(KalmanSOC_GetPersistentState, const KalmanSOC*, KalmanSOC_PersistentState*)   \
    VALUE(uint32_t, KalmanSOC_CalculateCRC32, const KalmanSOC_PersistentState*)        \
    VALUE(bool, KalmanSOC_ValidatePersistentState, const KalmanSOC_PersistentState*)   \
    VOID(KalmanSOC_Reset, KalmanSOC*)                                                  \
    VOID(KalmanSOC_GetEstimate, const KalmanSOC*, SOC_Estimate*)                       \
    VALUE(bool, KalmanSOC_IsHealthy, const KalmanSOC*)                                 \
    VOID(KalmanSOC_GetStats, const KalmanSOC*, float*, float*)
// clang-format on

FAKE_KALMAN_SOC_LIST(DECLARE_FAKE_VALUE_FUNC, DECLARE_FAKE_VOID_FUNC)

// Reset every fake above.
void fake_kalman_soc_reset(void);

#endif

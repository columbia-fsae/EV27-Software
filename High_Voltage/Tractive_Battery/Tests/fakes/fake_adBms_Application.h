/**
 * @file fake_adBms_Application.h
 * @brief Use when testing code that drives the ADBMS application (main-loop logic,
 * can_management's pack messages) with pack data set directly by the test.
 *
 * FFF fakes for the functions in adBms_Application.h that adBms_Application.c
 * defines, plus every global it owns, which fake_adBms_Application_reset()
 * zeroes. The header also declares about 60 adBms6830_* functions from the ADI
 * example code that are defined nowhere; those are not faked. Add
 * `FAKES adBms_Application` to add_fw_test() to use it.
 *
 * Fill PackSegments / TotalPack / g_soc_estimate directly in a test to give
 * the code under test pack data.
 *
 * custom_fake needed:
 *   - measurement_loop / adBms_main_run / adBms6830_soc_run: they update the
 *     globals above on the target; do that in a custom_fake if the code under
 *     test reads them back after the call.
 */
#ifndef FAKE_ADBMS_APPLICATION_H
#define FAKE_ADBMS_APPLICATION_H

#include "adBms_Application.h"
#include "fff.h"

// clang-format off
#define FAKE_ADBMS_APPLICATION_LIST(VALUE, VOID)                      \
    VOID(measurement_loop, volatile CAN_Inputs_t*)                    \
    VOID(adbms_main_init, volatile CAN_Inputs_t*)                     \
    VOID(adBms_main_run, volatile CAN_Inputs_t*, GPIO_Info_t*)        \
    VOID(adBms6830_soc_run, volatile CAN_Inputs_t*)                   \
    VOID(adBms6830_soc_init)                                          \
    VOID(adBms6830_soc_update, int, int, float, float, float)         \
    VOID(adBms6830_print_soc_estimate, int, int, const SOC_Estimate*) \
    VOID(adBms6830_soc_save_to_flash)                                 \
    VOID(adBms6830_soc_clear_flash)                                   \
    VALUE(int, cell_to_temp_index, int)                               \
    VALUE(int, lookupID, uint64_t)
// clang-format on

FAKE_ADBMS_APPLICATION_LIST(DECLARE_FAKE_VALUE_FUNC, DECLARE_FAKE_VOID_FUNC)

// Globals adBms_Application.c defines without declaring them in its header
// (PackSegments, TotalPack and g_soc_estimate are declared there).
extern bool g_charger_present;
extern cell_asic IC[TOTAL_IC];
extern int g_active_cells[TOTAL_IC];
extern KalmanSOC g_kalman_soc[TOTAL_MODULES][CELLS_PER_MOD];
extern bool g_soc_initialized[TOTAL_MODULES][CELLS_PER_MOD];
extern uint32_t g_soc_update_counter[TOTAL_MODULES][CELLS_PER_MOD];
extern const float OV_THRESHOLD;
extern const float UV_THRESHOLD;
extern const float OT_THRESHOLD;
extern const float VOLTAGE_LSB;
extern const float BAL_THRESHOLD_V;
extern const float V_TEMP_TABLE[TEMP_TABLE_SIZE];

// Reset every fake above and zero the non-const globals.
void fake_adBms_Application_reset(void);

#endif

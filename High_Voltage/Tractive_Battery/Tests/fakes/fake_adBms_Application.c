#include "fake_adBms_Application.h"

#include <string.h>

FAKE_ADBMS_APPLICATION_LIST(DEFINE_FAKE_VALUE_FUNC, DEFINE_FAKE_VOID_FUNC)

// Globals owned by the real adBms_Application.c
bool g_charger_present;
cell_asic IC[TOTAL_IC];
SegmentData_t PackSegments[TOTAL_MODULES];
TotalPack_t TotalPack;
int g_active_cells[TOTAL_IC];
KalmanSOC g_kalman_soc[TOTAL_MODULES][CELLS_PER_MOD];
SOC_Estimate g_soc_estimate[TOTAL_MODULES][CELLS_PER_MOD];
bool g_soc_initialized[TOTAL_MODULES][CELLS_PER_MOD];
uint32_t g_soc_update_counter[TOTAL_MODULES][CELLS_PER_MOD];

// Same values as the real module (adbms_config.h)
const float OV_THRESHOLD = CELL_OV_THRESHOLD_V;
const float UV_THRESHOLD = CELL_UV_THRESHOLD_V;
const float OT_THRESHOLD = CELL_OT_THRESHOLD_C;
const float VOLTAGE_LSB = ADBMS_VOLTAGE_LSB_V;
const float BAL_THRESHOLD_V = BAL_HYSTERESIS_V;
const float V_TEMP_TABLE[TEMP_TABLE_SIZE] = TEMP_TABLE_V_DATA;

#define RESET_VALUE_FAKE(rtype, name, ...) RESET_FAKE(name);
#define RESET_VOID_FAKE(name, ...)         RESET_FAKE(name);

void fake_adBms_Application_reset(void) {
    FAKE_ADBMS_APPLICATION_LIST(RESET_VALUE_FAKE, RESET_VOID_FAKE)
    g_charger_present = false;
    memset(IC, 0, sizeof(IC));
    memset(PackSegments, 0, sizeof(PackSegments));
    memset(&TotalPack, 0, sizeof(TotalPack));
    memset(g_active_cells, 0, sizeof(g_active_cells));
    memset(g_kalman_soc, 0, sizeof(g_kalman_soc));
    memset(g_soc_estimate, 0, sizeof(g_soc_estimate));
    memset(g_soc_initialized, 0, sizeof(g_soc_initialized));
    memset(g_soc_update_counter, 0, sizeof(g_soc_update_counter));
}

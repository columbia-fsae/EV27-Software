/**
 * @file fake_can_management.h
 * @brief Use when testing code that sends CAN frames or reads can_data (adBms_Application,
 * main-loop logic) without the real CAN encoding or the HAL FDCAN driver.
 *
 * FFF fakes for every function in can_management.h, plus the globals the real
 * can_management.c owns (can_data, error_info), which fake_can_management_reset()
 * zeroes. Add `FAKES can_management` to add_fw_test() to use it.
 *
 * custom_fake needed:
 *   - CAN_SendData: `data` points at the caller's stack buffer. To check the
 *     payload, copy it inside a custom_fake (see TUTORIAL.md, pointer arguments).
 *   - The bms_/soc_ functions with counter pointers (bms_can_data, soc_can_data)
 *     only advance *_counter in a custom_fake.
 */
#ifndef FAKE_CAN_MANAGEMENT_H
#define FAKE_CAN_MANAGEMENT_H

#include "can_management.h"
#include "fff.h"

typedef SOC_Estimate (*fake_soc_rows_t)[CELLS_PER_MOD];

// clang-format off
#define FAKE_CAN_MANAGEMENT_LIST(VALUE, VOID)                                                   \
    VOID(can_init, FDCAN_HandleTypeDef*, GPIO_Info_t*)                                          \
    VOID(bms_can_stats, SegmentData_t*, TotalPack_t*, FDCAN_HandleTypeDef*)                     \
    VOID(bms_can_faults, SegmentData_t*, TotalPack_t*, FDCAN_HandleTypeDef*)                    \
    VOID(bms_can_data, SegmentData_t*, TotalPack_t*, FDCAN_HandleTypeDef*, uint8_t*, uint8_t*)  \
    VOID(bsm_can, bsm_obj*, GPIO_Info_t*, FDCAN_HandleTypeDef*)                                 \
    VOID(soc_can_stats, SegmentData_t*, fake_soc_rows_t, TotalPack_t*, FDCAN_HandleTypeDef*)    \
    VOID(bms_can_ids, SegmentData_t*, fake_soc_rows_t, TotalPack_t*, FDCAN_HandleTypeDef*)      \
    VOID(soc_can_data, fake_soc_rows_t, TotalPack_t*, FDCAN_HandleTypeDef*, uint8_t*, uint8_t*) \
    VOID(error_can, FDCAN_HandleTypeDef*)                                                       \
    VOID(adc_can, ADC_Inputs_t*, FDCAN_HandleTypeDef*)                                          \
    VALUE(HAL_StatusTypeDef, CAN_SendData, uint16_t, uint8_t*, uint32_t, FDCAN_HandleTypeDef*)
// clang-format on

FAKE_CAN_MANAGEMENT_LIST(DECLARE_FAKE_VALUE_FUNC, DECLARE_FAKE_VOID_FUNC)

void fake_can_management_reset(void);

#endif

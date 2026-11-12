/**
 * @file fake_adbms_driver.h
 * @brief Use when testing adBms_Application: stands in for the ADBMS6830 chip, its
 * SPI driver library (ADBMS6830/lib) and mcuWrapper.c.
 *
 * FFF fakes for the driver functions adBms_Application.c calls. This is a
 * pseudo-module: * the functions come from adBms6830GenericType.c, adBms6830ParseCreate.c and
 * mcuWrapper.c. None of the globals those files define are used by
 * adBms_Application.c, so none are defined here. Add `FAKES adbms_driver` to
 * add_fw_test() to use it. If a new caller needs another driver function, add
 * it to the list.
 *
 * custom_fake needed:
 *   - adBmsReadData: the chip "answers" by filling ic[] (cell voltages, aux
 *     temperatures, status, PEC error counts). Write the readings into ic[]
 *     for the requested `type` in a custom_fake.
 *   - adBmsWriteData: copy ic[] in a custom_fake to check what was written.
 *   - SetOverVoltageThreshold / SetUnderVoltageThreshold: return_val is the
 *     register code; use a custom_fake to compute it from the voltage.
 */
#ifndef FAKE_ADBMS_DRIVER_H
#define FAKE_ADBMS_DRIVER_H

#include "adBms6830GenericType.h"
#include "adBms6830ParseCreate.h"
#include "fff.h"
#include "mcuWrapper.h"

// clang-format off
#define FAKE_ADBMS_DRIVER_LIST(VALUE, VOID)                        \
    VOID(Delay_ms, uint32_t)                                       \
    VOID(adBmsWakeupIc, uint8_t)                                   \
    VOID(adBmsReadData, uint8_t, cell_asic*, uint8_t*, TYPE, GRP)  \
    VOID(adBmsWriteData, uint8_t, cell_asic*, uint8_t*, TYPE, GRP) \
    VOID(adBms6830_Adcv, RD, CONT, DCP, RSTF, OW_C_S)              \
    VOID(adBms6830_Adax, OW_AUX, PUP, CH)                          \
    VOID(adBms6830_Adax2, CH)                                      \
    VOID(adBms6830_Snap)                                           \
    VOID(adBms6830_Unsnap)                                         \
    VALUE(uint16_t, SetOverVoltageThreshold, float)                \
    VALUE(uint16_t, SetUnderVoltageThreshold, float)
// clang-format on

FAKE_ADBMS_DRIVER_LIST(DECLARE_FAKE_VALUE_FUNC, DECLARE_FAKE_VOID_FUNC)

void fake_adbms_driver_reset(void);

#endif

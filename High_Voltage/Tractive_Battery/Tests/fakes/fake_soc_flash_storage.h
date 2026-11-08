/**
 * @file fake_soc_flash_storage.h
 * @brief Use when testing SOC save/restore logic (adBms_Application) without
 * flash: simulate empty, valid, corrupt or failing flash.
 *
 * FFF fakes for every function in soc_flash_storage.h. soc_flash_storage.c
 * owns no global variables. Add `FAKES soc_flash_storage` to add_fw_test() to
 * use it.
 *
 * custom_fake needed:
 *   - SOC_Flash_Load fills *persistent only in a custom_fake (copy a saved
 *     KalmanSOC_PersistentState into it and return true).
 *   - SOC_Flash_Save: to check what was saved, copy *persistent in a
 *     custom_fake; the pointer may be to a stack variable.
 */
#ifndef FAKE_SOC_FLASH_STORAGE_H
#define FAKE_SOC_FLASH_STORAGE_H

#include "fff.h"
#include "soc_flash_storage.h"

// clang-format off
#define FAKE_SOC_FLASH_STORAGE_LIST(VALUE, VOID)                  \
    VALUE(bool, SOC_Flash_Save, const KalmanSOC_PersistentState*) \
    VALUE(bool, SOC_Flash_Load, KalmanSOC_PersistentState*)       \
    VALUE(bool, SOC_Flash_Erase)                                  \
    VALUE(bool, SOC_Flash_HasValidData)
// clang-format on

FAKE_SOC_FLASH_STORAGE_LIST(DECLARE_FAKE_VALUE_FUNC, DECLARE_FAKE_VOID_FUNC)

// Reset every fake above.
void fake_soc_flash_storage_reset(void);

#endif

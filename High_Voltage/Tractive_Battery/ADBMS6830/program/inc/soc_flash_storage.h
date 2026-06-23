/**
 * @file soc_flash_storage.h
 * @brief Flash storage for Kalman SOC persistence on STM32G4
 * 
 * Uses last page of flash (Bank 2, Page 127) for persistent storage
 * NUCLEO-G474RE: 512KB flash, 2KB pages
 */

#ifndef SOC_FLASH_STORAGE_H
#define SOC_FLASH_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "kalman_soc.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// FLASH STORAGE API
// ============================================================================

/**
 * @brief Save SOC state to flash
 * @param persistent State to save
 * @return true if successful
 */
bool SOC_Flash_Save(const KalmanSOC_PersistentState *persistent);

/**
 * @brief Load SOC state from flash
 * @param persistent Output state structure
 * @return true if valid data loaded
 */
bool SOC_Flash_Load(KalmanSOC_PersistentState *persistent);

/**
 * @brief Erase SOC flash storage
 * @return true if successful
 */
bool SOC_Flash_Erase(void);

/**
 * @brief Check if valid SOC data exists in flash
 * @return true if valid data present
 */
bool SOC_Flash_HasValidData(void);

#ifdef __cplusplus
}
#endif

#endif // SOC_FLASH_STORAGE_H

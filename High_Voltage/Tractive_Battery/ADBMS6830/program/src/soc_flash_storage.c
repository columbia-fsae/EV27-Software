/**
 * @file soc_flash_storage.cpp
 * @brief Flash storage implementation for STM32G4
 * 
 * For NUCLEO-G474RE (STM32G474RET6):
 * - 512KB flash total
 * - 2KB pages (256 pages total)
 * - Dual bank: Bank 1 (0-127), Bank 2 (128-255)
 * - We use Bank 2, Page 127 (last page of Bank 1 to avoid bootloader area)
 */

#include "soc_flash_storage.h"
#include "kalman_soc.h"
#include "kalman_soc_config.h"
#include <string.h>
#ifdef STM32G474xx
#include "stm32g4xx_hal.h"
#endif

// Flash address for SOC storage
// Bank 1, Page 127 (last page before Bank 2)
// Address = 0x08000000 + (127 * 2048) = 0x0803F800
#define SOC_FLASH_ADDRESS   0x0803F800UL
#define SOC_FLASH_PAGE      127
#define SOC_FLASH_BANK      FLASH_BANK_1

bool SOC_Flash_Save(const KalmanSOC_PersistentState *persistent)
{
#ifdef STM32G4
    HAL_StatusTypeDef status;
    
    // Unlock flash
    HAL_FLASH_Unlock();
    
    // Erase page first
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;
    
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.Banks = SOC_FLASH_BANK;
    erase_init.Page = SOC_FLASH_PAGE;
    erase_init.NbPages = 1;
    
    status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    if (status != HAL_OK) {
        HAL_FLASH_Lock();
        return false;
    }
    
    // Write data (8 bytes at a time for STM32G4)
    uint64_t *data_ptr = (uint64_t*)persistent;
    uint32_t num_dwords = (sizeof(KalmanSOC_PersistentState) + 7) / 8;
    
    for (uint32_t i = 0; i < num_dwords; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, 
                                   SOC_FLASH_ADDRESS + (i * 8),
                                   data_ptr[i]);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    
    // Lock flash
    HAL_FLASH_Lock();
    
    return true;
#else
    // Non-STM32 platform - return false
    return false;
#endif
}

bool SOC_Flash_Load(KalmanSOC_PersistentState *persistent)
{
    // Read from flash
    const KalmanSOC_PersistentState *flash_data = (const KalmanSOC_PersistentState*)SOC_FLASH_ADDRESS;
    
    // Copy to RAM
    memcpy(persistent, flash_data, sizeof(KalmanSOC_PersistentState));
    
    // Validate
    return KalmanSOC_ValidatePersistentState(persistent);
}

bool SOC_Flash_Erase(void)
{
#ifdef STM32G474xx
    HAL_StatusTypeDef status;
    
    HAL_FLASH_Unlock();
    
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;
    
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.Banks = SOC_FLASH_BANK;
    erase_init.Page = SOC_FLASH_PAGE;
    erase_init.NbPages = 1;
    
    status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    
    HAL_FLASH_Lock();
    
    return (status == HAL_OK);
#else
    return false;
#endif
}

bool SOC_Flash_HasValidData(void)
{
    const KalmanSOC_PersistentState *flash_data = (const KalmanSOC_PersistentState*)SOC_FLASH_ADDRESS;
    return KalmanSOC_ValidatePersistentState(flash_data);
}

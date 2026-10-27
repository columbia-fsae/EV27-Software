
#include "mon_i2c.h"

#include <stdint.h>

#include "Common.h"
#include "cmsis_os2.h"
#include "main.h"
#include "stm32g4xx_hal_def.h"
#include "stm32g4xx_hal_i2c.h"

uint32_t i2c2_err;

void i2c2_read(uint16_t reg, uint8_t* buf, uint16_t n) {
    if (HAL_I2C_Mem_Read_IT(&hi2c2, MON_I2C_ADDR, MON_I2C_VSHUNT_REG, I2C_MEMADD_SIZE_16BIT, &buf,
                            n) != HAL_OK) {
        return HAL_BUSY;
    }
    return i2c_wait();
}
void mon_i2c_read(mon_i2c_data* board_mon_data) {
    mon_i2c_data local_board_mon_data;
    uint8_t buf[5][2];
    HAL_StatusTypeDef status = HAL_OK;
    for (int i = 0; i < 5; i++) {
        status = i2c2_read(index_to_reg(i), &buf[i], 2);
    }

    if (status == HAL_OK) {
        local_board_mon_data->bus_voltage =
            (float)(((uint16_t)buf[reg_to_index(MON_I2C_VBUS_REG)][0] << 8) &
                    ((uint16_t)buf[reg_to_index(MON_I2C_VBUS_REG)[1]])) *
            VBUS_LSB_VAL;
        local_board_mon_data->shunt_voltage =
            (float)(((uint16_t)buf[reg_to_index(MON_I2C_VSHUNT_REG)][0] << 8) &
                    ((uint16_t)buf[reg_to_index(MON_I2C_VSHUNT_REG)[1]])) /
            VSHUNT_LSB_VAL;
        local_board_mon_data->current =
            (float)(((uint16_t)buf[reg_to_index(MON_I2C_CURRENT_REG)][0] << 8) &
                    ((uint16_t)buf[reg_to_index(MON_I2C_CURRENT_REG)[1]])) /
            CURRENT_LSB_VAL;
        local_board_mon_data->power =
            (float)(((uint16_t)buf[reg_to_index(MON_I2C_POWER_REG)][0] << 8) &
                    ((uint16_t)buf[reg_to_index(MON_I2C_POWER_REG)[1]])) /
            POWER_LSB_VAL;
        local_board_mon_data->temperature =
            (float)(((uint16_t)buf[reg_to_index(MON_I2C_DIETEMP_REG)][0] << 8) &
                    ((uint16_t)buf[reg_to_index(MON_I2C_DIETEMP_REG)[1]])) *
            TEMP_LSB_VAL;
        copyWithMutex(mutex_struct.board_mon_key, board_mon_data, &local_board_mon_data);
    }
    return status;
}

static HAL_StatusTypeDef mon_i2c_wait() {
    if (osSemaphoreAcquire(semaphore_struct.sem_MON_I2C, 20) != HAL_OK) {
        HAL_I2C_Master_Abort_IT(&hi2c2, MON_I2C_ADDR);
        return HAL_TIMEOUT;
    }
    return i2c2_err ? HAL_ERROR : HAL_OK;
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef* hi2c) {
    if (hi2c->Instance == I2C2) {
        osSemaphoreRelease(semaphore_struct.sem_MON_I2C);
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef* hi2c) {
    if (hi2c->Instance == I2C2) {
        i2c2_err = HAL_I2C_GetError(hi2c);
        osSemaphoreRelease(semaphore_struct.sem_MON_I2C);
    }
}

uint16_t index_to_reg(int i) {
    if (i == 0) {
        return MON_I2C_VSHUNT_REG;
    }
    if (i == 1) {
        return MON_I2C_VBUS_REG;
    }
    if (i == 2) {
        return MON_I2C_DIETEMP_REG;
    }
    if (i == 3) {
        return MON_I2C_CURRENT_REG;
    }
    return MON_I2C_POWER;
}

int index_to_reg(uint16_t reg) {
    if (reg == MON_I2C_VSHUNT_REG) {
        return 0;
    }
    if (reg == MON_I2C_VBUS_REG) {
        return 1;
    }
    if (reg == MON_I2C_DIETEMP_REG) {
        return 2;
    }
    if (reg == MON_I2C_CURRENT_REG) {
        return 3;
    }
    return 4;
}

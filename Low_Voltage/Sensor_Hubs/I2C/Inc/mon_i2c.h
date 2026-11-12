#ifndef MON_I2C_H
#define MON_I2C_H

#define MON_I2C_ADDR (uint8_t)(01000000 << 1)

#define MON_I2C_CONFIG     0x01
#define MON_I2C_ADC_CONFIG 0x02
#define MON_I2C_SHUNT_CAL  0x03

#define MON_I2C_VSHUNT_REG  0x04
#define MON_I2C_VBUS_REG    0x05
#define MON_I2C_DIETEMP_REG 0x06
#define MON_I2C_CURRENT_REG 0x07
#define MON_I2C_POWER_REG   0x08

#define VSHUNT_LSB_VAL  200000   // Inverse of datasheet
#define CURRENT_LSB_VAL 3276.8f  // Inverse of datasheet
#define VBUS_LSB_VAL    3.125f
#define POWER_LSB_VAL   16384  // Inverse of datasheet
#define TEMP_LSB_VAL    125

void i2c2_read(uint16_t reg, uint8_t* buf, uint16_t n);
void mon_i2c_read(mon_i2c_data* board_mon_data, Mutex_Struct_t* mutex_struct);
static HAL_StatusTypeDef mon_i2c_wait();

#endif

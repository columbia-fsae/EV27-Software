/*******************************************************************************
Copyright (c) 2020 - Analog Devices Inc. All Rights Reserved.
This software is proprietary & confidential to Analog Devices, Inc.
and its licensor.
******************************************************************************
* @file:    adBms_Application.h
* @brief:   Bms application header file
* @version: $Revision$
* @date:    $Date$
* Developed by: ADIBMS Software team, Bangalore, India
*****************************************************************************/
/*! @addtogroup APPLICATION
*  @{
*
*/

/*! @addtogroup APPLICATION
*  @{
*
*/

#ifndef __APPLICATION_H
#define __APPLICATION_H

#include <stdint.h>
#include "adbms_main.h"
#include "kalman_soc.h"
#include "common_types.h"

#define TOTAL_IC 6

// SOC CHANGES
#define CELL_TIE_TOLERANCE 0.0002f
#define TOTAL_MODULES 6 // E.g., 12 ICs = 6 Segments
#define TOTAL_CELLS TOTAL_MODULES*CELLS_PER_MOD
#define MIN_CELL_THRESH     0.2
#define SENSOR_DROPOUT_TEMP     -99
#define TEMP_PER_BOARD 10
#define VOLT_PER_SLAVE 10
#define VOLT_PER_MASTER 14
#define COMM_FAULT_LATCH 3   // consecutive bad cycles before hard fault


//ADBMS6830 Unique 48 Bit IDs
#define ID1_TOP 0x97DE40006B21ULL
#define ID2_TOP 0x997CE40586B2ULL
#define ID3_TOP 0x097EE40006B2ULL
#define ID4_TOP 0x990DE40686B2ULL
#define ID5_TOP 0xC97DE40506B2ULL
#define ID6_TOP 0x590EE40586B2ULL
#define ID7_TOP 0x9A06E40606B2ULL
#define ID1_BOTTOM 0x0001111111ULL
#define ID2_BOTTOM 0x0002111111ULL
#define ID3_BOTTOM 0x0003111111ULL
#define ID4_BOTTOM 0x0004111111ULL
#define ID5_BOTTOM 0x0005111111ULL
#define ID6_BOTTOM 0x0006111111ULL
#define ID7_BOTTOM 0x0001111111ULL

void app_main(void);
void run_command(int cmd);
void adBms6830_init_config(uint8_t tIC, cell_asic *ic);
void adBms6830_write_read_config(uint8_t tIC, cell_asic *ic);
void adBms6830_write_config(uint8_t tIC, cell_asic *ic);
void adBms6830_read_config(uint8_t tIC, cell_asic *ic);
void adBms6830_start_adc_cell_voltage_measurment(uint8_t tIC);
void adBms6830_read_cell_voltages(uint8_t tIC, cell_asic *ic);
void adBms6830_start_adc_s_voltage_measurment(uint8_t tIC);
void adBms6830_read_s_voltages(uint8_t tIC, cell_asic *ic);
void adBms6830_start_avgcell_voltage_measurment(uint8_t tIC);
void adBms6830_read_avgcell_voltages(uint8_t tIC, cell_asic *ic);
void adBms6830_start_fcell_voltage_measurment(uint8_t tIC);
void adBms6830_read_fcell_voltages(uint8_t tIC, cell_asic *ic);
void adBms6830_start_aux_voltage_measurment(uint8_t tIC, cell_asic *ic);
void adBms6830_read_aux_voltages(uint8_t tIC, cell_asic *ic);
void adBms6830_start_raux_voltage_measurment(uint8_t tIC, cell_asic *ic);
void adBms6830_read_raux_voltages(uint8_t tIC, cell_asic *ic);
void adBms6830_read_status_registers(uint8_t tIC, cell_asic *ic);
void measurement_loop(volatile CAN_Inputs_t *can_data);
void adBms6830_read_device_sid(uint8_t tIC, cell_asic *ic);
void adBms6830_set_reset_gpio_pins(uint8_t tIC, cell_asic *ic);
void adBms6830_enable_mute(uint8_t tIC, cell_asic *ic);
void adBms6830_disable_mute(uint8_t tIC, cell_asic *ic);
void adBms6830_soft_reset(uint8_t tIC);
void adBms6830_reset_cmd_count(uint8_t tIC);
void adBms6830_reset_pec_error_flag(uint8_t tIC, cell_asic *ic);
void adBms6830_snap(uint8_t tIC);
void adBms6830_unsnap(uint8_t tIC);
void adBms6830_clear_cell_measurement(uint8_t tIC);
void adBms6830_clear_aux_measurement(uint8_t tIC);
void adBms6830_clear_spin_measurement(uint8_t tIC);
void adBms6830_clear_fcell_measurement(uint8_t tIC);
void adBms6830_clear_ovuv_measurement(uint8_t tIC);
void adBms6830_clear_all_flags(uint8_t tIC, cell_asic *ic);
void adBms6830_set_dcc_discharge(uint8_t tIC, cell_asic *ic);
void adBms6830_clear_dcc_discharge(uint8_t tIC, cell_asic *ic);
void adBms6830_write_read_pwm_duty_cycle(uint8_t tIC, cell_asic *ic);
void adBms6830_gpio_spi_communication(uint8_t tIC, cell_asic *ic);
void adBms6830_gpio_i2c_write_to_slave(uint8_t tIC, cell_asic *ic);
void adBms6830_gpio_i2c_read_from_slave(uint8_t tIC, cell_asic *ic);
void adBms6830_set_dtrng_dcto_value(uint8_t tIC, cell_asic *ic);
void adBms6830_run_osc_mismatch_self_test(uint8_t tIC, cell_asic *ic);
void adBms6830_run_thermal_shutdown_self_test(uint8_t tIC, cell_asic *ic);
void adBms6830_run_supply_error_detection_self_test(uint8_t tIC, cell_asic *ic);
void adBms6830_run_thermal_shutdown_self_test(uint8_t tIC, cell_asic *ic);
void adBms6830_run_fuse_ed_self_test(uint8_t tIC, cell_asic *ic);
void adBms6830_run_fuse_med_self_test(uint8_t tIC, cell_asic *ic);
void adBms6830_run_tmodchk_self_test(uint8_t tIC, cell_asic *ic);
void adBms6830_check_latent_fault_csflt_status_bits(uint8_t tIC, cell_asic *ic);
void adBms6830_check_rdstatc_err_bit_functionality(uint8_t tIC, cell_asic *ic);
void adBms6830_cell_openwire_test(uint8_t tIC, cell_asic *ic);
void adBms6830_redundant_cell_openwire_test(uint8_t tIC, cell_asic *ic);
void adBms6830_cell_ow_volatge_collect(uint8_t tIC, cell_asic *ic, TYPE type, OW_C_S ow_c_s);
void adBms6830_aux_openwire_test(uint8_t tIC, cell_asic *ic);
void adBms6830_gpio_pup_up_down_volatge_collect(uint8_t tIC, cell_asic *ic, PUP pup);
void adBms6830_open_wire_detection_condtion_check(uint8_t tIC, cell_asic *ic, TYPE type);
void adBms6830_read_rdcvall_voltage(uint8_t tIC, cell_asic *ic);
void adBms6830_read_rdacall_voltage(uint8_t tIC, cell_asic *ic);
void adBms6830_read_rdsall_voltage(uint8_t tIC, cell_asic *ic);
void adBms6830_read_rdfcall_voltage(uint8_t tIC, cell_asic *ic);
void adBms6830_read_rdcsall_voltage(uint8_t tIC, cell_asic *ic);
void adBms6830_read_rdacsall_voltage(uint8_t tIC, cell_asic *ic);
void adBms6830_read_rdasall_voltage(uint8_t tIC, cell_asic *ic);

//Main Functions
void adbms_main_init(volatile CAN_Inputs_t *can_data);
void adBms_main_run(volatile CAN_Inputs_t *can_data, GPIO_Info_t *gpio_data);

//SOC Functions
void adBms6830_soc_run(volatile CAN_Inputs_t *can_data);
void adBms6830_soc_init(void);
void adBms6830_soc_update(int cic, int cell, float cell_voltage, float pack_current, float cell_temp);
void adBms6830_print_soc_estimate(int cic, int cell, const SOC_Estimate *est);
void adBms6830_soc_save_to_flash(void);
void adBms6830_soc_clear_flash(void);

//Helper Functions
int cell_to_temp_index(int cell_i);
int lookupID(uint64_t id);

//External Items needed by Main
extern SegmentData_t PackSegments[TOTAL_MODULES];
extern TotalPack_t TotalPack;
extern SOC_Estimate g_soc_estimate[TOTAL_MODULES][CELLS_PER_MOD];


#endif
/** @}*/
/** @}*/

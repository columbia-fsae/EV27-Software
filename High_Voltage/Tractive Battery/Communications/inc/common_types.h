//File to store structuer so that multiple files can reference it without dependency issues
#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#define CELLS_PER_MOD 10
#define TEMP_PER_MOD 10

#include <stdint.h>
#include <stdbool.h>

//GPIO Storage Structure
typedef struct {
    bool ir_plus_aux;
    bool ir_minus_aux;
    bool slow_CAN;
    bool sdc_ok;
    bool bms_ok_OUT;
    bool mcu_mhs;
    bool mcu_mls;
} GPIO_Info_t;

//Battery State Machine Attribute Structure
typedef struct BsmTag {
    int8_t   state;
    uint32_t timer;
    bool     pc_enable;
    bool     ir_plus_enable;
    bool     ir_minus_enable;
} bsm_obj;

//CAN Input Signals (written by RX callback, read by state machine)
typedef struct {
    //FROM INVERTER
	volatile float tractive_current;  //From Charger in the case of the charger being plugged in
    volatile float dc_bus_voltage;

    //FROM CHARGER
    volatile bool balancing_enable;
} CAN_Inputs_t;

/* --- GLOBAL PACK STATUS  */
typedef struct {
    uint16_t cell_v_mV[CELLS_PER_MOD];  	// Voltages scaled to millivolts (e.g., 4125 = 4.125V)
    float   temp_C[20];     			// Temperatures in Celsius (e.g., 45 = 45C)
    uint32_t dcc_active;     			// 24-bit mask for balancing states
    uint8_t  fault_flags;    			// Bit 0: OV, Bit 1: UV, Bit 2: OT, Bit 3: Comm Drop
    uint8_t id[2];
} SegmentData_t;

/* --- GLOBAL OVERALL PACK INFO (with SOC) --- */
typedef struct{
	//BMS
	float voltage;
	float avg_voltage;
	float temp;
	bool balancing_done;

	//SOC
	float soc;
	float capacity;
	float uncertainty;
} TotalPack_t;

//State of Charger Estimation Outputs
typedef struct {
	//SOC State Variables
    float soc;
    float soc_percent;
    float v_ct;
    float v_dif;

    //Lookup Quantities
    float voltage_ocv;

    //Parameter State Variables
    float r0;
    float capacity_ah;

    //Other Outputs
    float soh_percent;
    bool valid;
    float confidence;
} SOC_Estimate;

extern volatile CAN_Inputs_t can_data;

#endif

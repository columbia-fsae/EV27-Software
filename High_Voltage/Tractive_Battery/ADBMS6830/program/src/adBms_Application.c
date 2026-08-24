/*******************************************************************************
* @file:    adbms_Application.c
* @brief:   STM32CubeIDE Real-Time ADBMS6830 Application
*****************************************************************************/
#include "main.h"
#include "common.h"
#include "adbms_main.h"
#include "adBms_Application.h"
#include "adBms6830CmdList.h"
#include "adBms6830GenericType.h"
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

// SOC CHANGES
#include "kalman_soc.h"
#include "battery_model.h"
#include "soc_flash_storage.h"
#include "can_management.h"

bool g_charger_present = false;
/* --- SYSTEM CONSTANTS & THRESHOLDS --- */

cell_asic IC[TOTAL_IC];

const float OV_THRESHOLD     = 4.25f;    // Overvoltage limit
const float UV_THRESHOLD     = 2.5f;    // Undervoltage limit
const float OT_THRESHOLD     = 60.0f;   // Overtemperature limit (Celsius)
const float VOLTAGE_LSB      = 0.00015f;// ADBMS6830 ADC LSB is 150uV
const float BAL_THRESHOLD_V = 0.05f;  // 5mV hysteresis — balance if cell is this far above min

/* --- GLOBAL PACK STATUS --- */
SegmentData_t PackSegments[TOTAL_MODULES];
TotalPack_t   TotalPack;
int           g_active_cells[TOTAL_IC];
KalmanSOC
g_kalman_soc[TOTAL_MODULES][CELLS_PER_MOD];
SOC_Estimate g_soc_estimate[TOTAL_MODULES][CELLS_PER_MOD];
bool         g_soc_initialized[TOTAL_MODULES][CELLS_PER_MOD];
uint32_t     g_soc_update_counter[TOTAL_MODULES][CELLS_PER_MOD];

static uint8_t comm_miss_count[TOTAL_MODULES] = {0};


int cell_to_temp_index(int cell_i);
void adBms6830_print_soc_estimate(int cic, int cell, const SOC_Estimate *est);
void adBms6830_soc_update(int cic, int cell, float cell_voltage, float pack_current, float cell_temp);
void adBms6830_soc_init(void);

//Tables for SOC estimation, from HPPC testing
static RC_LookupTable g_r_ct_lut = {
    .temp_points = {25.0f, 26.0f},
    .soc_points = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.0f},
    .values = {
        {0.0007293401782525250f, 0.0009074875325995350f, 0.00085638685177882f,
         0.0012423409779717100f, 0.0011932057340083800f, 0.001315810325699840f,
         0.0008616053304370760f, 0.0012148334716370600f, 0.0023088303124354100f,
         0.004822437902900810f},{0.0007293401782525250f, 0.0009074875325995350f, 0.00085638685177882f,
                 0.0012423409779717100f, 0.0011932057340083800f, 0.001315810325699840f,
                 0.0008616053304370760f, 0.0012148334716370600f, 0.0023088303124354100f,
                 0.004822437902900810f}
    }
};

static RC_LookupTable g_c_ct_lut = {
	.temp_points = {25.0f, 26.0f},
    .soc_points = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.0f},
    .values = {
        {3426.8040222159100f, 4055.2212079149300f, 4828.111556415460f,
         5634.523954468950f, 5866.549079080130f, 5319.9156924664600f,
         7077.136410907460f, 5762.106628957510f, 3031.838226611050f,
         1451.5479806985900f},{3426.8040222159100f, 4055.2212079149300f, 4828.111556415460f,
                 5634.523954468950f, 5866.549079080130f, 5319.9156924664600f,
                 7077.136410907460f, 5762.106628957510f, 3031.838226611050f,
                 1451.5479806985900f}
    }
};

static RC_LookupTable g_r_dif_lut = {
	.temp_points = {25.0f, 26.0f},
	.soc_points = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.0f},
    .values = {
        {0.0018522507006706300f, 0.0023720948484197800f, 0.002067557002357570f,
         0.00125871530333011f, 0.0014997750862781800f, 0.001823055274690330f,
         0.001862907879059510f, 0.00126993563273167f, 0.002673415576968640f,
         0.004061331586355210f},{0.0018522507006706300f, 0.0023720948484197800f, 0.002067557002357570f,
                 0.00125871530333011f, 0.0014997750862781800f, 0.001823055274690330f,
                 0.001862907879059510f, 0.00126993563273167f, 0.002673415576968640f,
                 0.004061331586355210f}
    }
};

static RC_LookupTable g_c_dif_lut = {
	.temp_points = {25.0f, 26.0f},
	.soc_points = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.0f},
    .values = {
        {10829.999076136800f, 14390.588433361300f, 17705.78890692150f,
         43171.568400062000f, 26935.39645917410f, 32112.190637379200f,
         22994.811807973600f, 31368.700780914500f, 21714.92126684060f,
         12507.460992043400f},{10829.999076136800f, 14390.588433361300f, 17705.78890692150f,
                 43171.568400062000f, 26935.39645917410f, 32112.190637379200f,
                 22994.811807973600f, 31368.700780914500f, 21714.92126684060f,
                 12507.460992043400f}
    }
};

static RC_LookupTable g_r0_lut = {
		.temp_points = {25.0f,26.0f},
		.soc_points = {0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.0f},
		.values = {
				{0.005309061985376070f, 0.005045924714300260f, 0.004985194918799730f,
				 0.005032740842521390f, 0.005065978524900830f, 0.00499394960654918f,
				 0.005033886178286290f, 0.005035637273081450f, 0.005094409985592770f,
				 0.0060653011513259000f},{0.005309061985376070f, 0.005045924714300260f, 0.004985194918799730f,
						 0.005032740842521390f, 0.005065978524900830f, 0.00499394960654918f,
						 0.005033886178286290f, 0.005035637273081450f, 0.005094409985592770f,
						 0.0060653011513259000f}
		}
};


/* --- TEMPERATURE SENSOR LOOKUP TABLE (Enepaq VTC5A) --- */
// Maps voltages to temperatures from -40°C to +120°C in 5°C increments
const float V_TEMP_TABLE[33] = {
    2.44, 2.42, 2.40, 2.38, 2.35, 2.32, 2.27, 2.23, 2.17, 2.11, // -40 to 5
    2.05, 1.99, 1.92, 1.86, 1.80, 1.74, 1.68, 1.63, 1.59, 1.55, // 10 to 55
    1.51, 1.48, 1.45, 1.43, 1.40, 1.38, 1.37, 1.35, 1.34, 1.33, // 60 to 105
    1.32, 1.31, 1.30                                            // 110 to 120
};


/* -------------------------------------------------------------------------- */
/* Helper: Convert Zener Voltage to Celsius */
/* -------------------------------------------------------------------------- */
float Convert_Zener_Voltage_To_Temp(float v) {
        // If voltage is much higher than your coldest value, it's probably floating.
    if (v > 2.8f) {
        return -99.0f; // Use -99 as an obvious "Fault / Disconnected" indicator
    }
    // 2. Cold saturation
    if (v >= V_TEMP_TABLE[0]) return -40.0f;

    // 3. Hot saturation (Short circuit check)
    if (v <= V_TEMP_TABLE[32]) return 120.0f;

    // 4. Linear Interpolation
    for (int i = 0; i < 32; i++) {
        if (v <= V_TEMP_TABLE[i] && v > V_TEMP_TABLE[i+1]) {
            float v_diff = V_TEMP_TABLE[i] - V_TEMP_TABLE[i+1];
            float v_offset = V_TEMP_TABLE[i] - v;
            float fraction = v_offset / v_diff;
            return (-40.0f + (i * 5.0f)) + (fraction * 5.0f);
        }
    }

    return -99.0f; // Fallback
}

static void Update_Charger_Status(void)
{
    // Assume charger drives PC6 High when plugged in
    g_charger_present =
        (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6) == GPIO_PIN_SET);
}

static void Update_BMS_OK_Output(GPIO_Info_t *gpio_data)
{
	bool any_fault = false;

	// Check all segments for ANY fault flags
	for (int mod = 0; mod < TOTAL_MODULES; mod++) {
		if (PackSegments[mod].fault_flags != 0) {
			any_fault = true;
	        break;
	    }
	}

    // BMS_OK = 1 only if no faults.
    if (!any_fault) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);   // BMS_OK = High
        gpio_data->bms_ok_OUT = true;
    } else {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET); // BMS_OK = Low
        gpio_data->bms_ok_OUT = false;
    }
}


/* -------------------------------------------------------------------------- */
/* Cell Balancing Logic (Passive, Weakest-Cell Algorithm)                     */
/* -------------------------------------------------------------------------- */
void Process_Cell_Balancing() {
	bool check_balance = false;
    for (int seg = 0; seg < TOTAL_MODULES; seg++) {

        int ic_master = seg * 2;
        int ic_slave  = (seg * 2) + 1;

        // Skip balancing if communication is dropped for this segment
        if (PackSegments[seg].fault_flags & 0x08) continue;

        // 1. Find the minimum (weakest) cell voltage across all 24 cells in the segment
        uint16_t min_v_mV = 9999;
        for (int i = 0; i < CELLS_PER_MOD; i++) {
            uint16_t v_mV = PackSegments[seg].cell_v_mV[i];
            if (v_mV > 1000 && v_mV < min_v_mV) { // >1V threshold (1000mV) to ignore empty channels
                min_v_mV = v_mV;
            }
        }

        // Convert the balancing hysteresis threshold from Volts to mV
        uint16_t bal_thresh_mV = (uint16_t)(BAL_THRESHOLD_V * 1000.0f);
        uint32_t segment_dcc_mask = 0;

        // 2. Build the 24-bit DCC bitmask
        //    Disable balancing entirely if any OT fault is present (safety)
        if ((PackSegments[seg].fault_flags & 0x04) == 0) { // 0x04 is the OT bit
            for (int i = 0; i < CELLS_PER_MOD; i++) {
                if (PackSegments[seg].cell_v_mV[i] > (min_v_mV + bal_thresh_mV)) {
                    segment_dcc_mask |= (1UL << i);
                }
            }
        }

        // Store the full 24-bit mask in the segment data for CAN/printing
        PackSegments[seg].dcc_active = segment_dcc_mask;

        // 3. Split the 24-bit mask back into the two hardware ICs
        // Master IC handles the first 14 cells (Bits 0-13)
        uint16_t master_dcc = (uint16_t)(segment_dcc_mask & 0x3FFF);
        IC[ic_master].tx_cfgb.dcc = master_dcc;

        // Slave IC handles the remaining 10 cells (Bits 14-23)
        // Shift right by 14 to align with the slave's cell 0
        uint16_t slave_dcc = (uint16_t)((segment_dcc_mask >> 14) & 0x03FF);
        IC[ic_slave].tx_cfgb.dcc = slave_dcc;
        if(segment_dcc_mask){
        	check_balance = false;
        } else {
        	check_balance = true;
        }
    }

    // 4. Push updated CFGB (with DCC bits) to all ICs
    TotalPack.balancing_done = check_balance;
    adBmsWakeupIc(TOTAL_IC);
    adBmsWriteData(TOTAL_IC, &IC[0], WRCFGB, Config, B);
}

/* -------------------------------------------------------------------------- */
/* Helper: Print System Status to Console */
/* -------------------------------------------------------------------------- */
void Print_Status_To_Console() {
    //printf("\033[2J\033[H"); // Clear terminal screen
    printf("==================================================\r\n");
    printf("           ADBMS6830 PACK MONITORING              \r\n");
    printf("==================================================\r\n");

    for (int mod = 0; mod < TOTAL_MODULES; mod++) {
        printf("\r\n--- SEGMENT %d STATUS ---\r\n", mod + 1);

        if (PackSegments[mod].fault_flags & 0x08) {
            printf("[!] CRITICAL FAULT: Segment %d Communication Dropped\r\n", mod + 1);
            continue;
        }

        if (PackSegments[mod].fault_flags & 0x01) printf("[!] FAULT: Overvoltage\r\n");
        if (PackSegments[mod].fault_flags & 0x02) printf("[!] FAULT: Undervoltage\r\n");
        if (PackSegments[mod].fault_flags & 0x04) printf("[!] FAULT: Overtemperature\r\n");

        printf("\r\n[Cell Voltages]:\r\n");
        for (int i = 0; i < CELLS_PER_MOD; i++) {
            float v = PackSegments[mod].cell_v_mV[i] / 1000.0f; // mV back to V
            printf(" Cell %02d: %6.4f V", i + 1, v);
            if ((i + 1) % 4 == 0) printf("\r\n");
        }

        printf("\r\n[Temperatures (Zener)]:\r\n");
        for (int i = 0; i < TEMP_PER_MOD; i++) {
            printf(" Temp %02d: %f C", i + 1, PackSegments[mod].temp_C[i]);
            if ((i + 1) % 5 == 0) printf("\r\n");
        }

        // Note: The Raw GPIO loop you had in the print function previously only
        // looked at IC[0]. Since the focus is now on segments, I removed it here
        // to save console space, but you can add it back iterating over IC[ic_master] if needed.
    }
    printf("\r\n==================================================\r\n");
}

/* -------------------------------------------------------------------------- */
/* Core Data Processing Function (Mapped to 24-Cell Segments)                 */
/* -------------------------------------------------------------------------- */
void Process_Board_Data() {
    float totalVoltage = 0;
    float avgTemp = 0;
    uint8_t dead_cells = 0;

    for (int seg = 0; seg < TOTAL_MODULES; seg++) {

        // The two ICs that make up this segment
    	//TODO: Uncomment the next two lines for FULL pack
        int ic_master = seg; //* 2 ;       // IC 0, 2, 4... (14 cells)
        //int ic_slave  = (seg * 2) + 1; // IC 1, 3, 5... (10 cells)


        PackSegments[seg].dcc_active = 0;

        //TODO: RUN ON TEST BENCH, COMMENT OUT SLAVE PART
        // --- 1. Check for Communication Dropout (PEC error) ---
        if (IC[ic_master].cccrc.cell_pec != 0 || IC[ic_master].cccrc.aux_pec != 0 ){//||
           // IC[ic_slave].cccrc.cell_pec  != 0 || IC[ic_slave].cccrc.aux_pec  != 0) {

        	if (comm_miss_count[seg] < 255) comm_miss_count[seg]++;

        	if (comm_miss_count[seg] >= COMM_FAULT_LATCH) {
        		PackSegments[seg].fault_flags |= 0x08;   // real comm fault
        	}
        	// else: leave fault_flags clear, KEEP last cycle's cell_v_mV / temp_C untouched
        	continue;   // skip re-parsing this cycle's corrupt data either way
        }

        PackSegments[seg].fault_flags = 0;
        // --- 2. Extract Cell Voltages ---
        //TODO: Partial Pack Code
        // Slave IC (10 Cells) -> Maps to cell_v_mV[0..9]
        for (int i = 0; i < VOLT_PER_SLAVE; i++) {
        	float v = ((int16_t)IC[ic_master].cell.c_codes[i] * VOLTAGE_LSB) + 1.5f;

        	if(i == 0 || i == VOLT_PER_SLAVE-1){
        		v += 0.1f;
        	}
        	/*
        	if (i == 0 || i == VOLT_PER_SLAVE-1){ //VOLTAGE OFFSET FOR FIRST AND LAST CELLS from FUSE
        		v += 0.1f;
        	}
        	*/
            PackSegments[seg].cell_v_mV[i] = (uint16_t)(v * 1000.0f);
            totalVoltage += v;
            if (v > 1.0f) {
            	if (v > OV_THRESHOLD) PackSegments[seg].fault_flags |= 0x01;
            	if (v < UV_THRESHOLD) PackSegments[seg].fault_flags |= 0x02;
            }
        }

        // --- 3. Extract Temperatures ---
    	// Slave IC (10 Temps) -> Maps to temp_C[0..9]
    	for (int i = 0; i < TEMP_PER_BOARD; i++) {
    	    float raw_v = ((int16_t)IC[ic_master].aux.a_codes[i]) * VOLTAGE_LSB + 1.5f;

    	    float t = Convert_Zener_Voltage_To_Temp(raw_v);

    	    if (t == SENSOR_DROPOUT_TEMP) {
    	    	dead_cells += 2;
    	    }
    	    PackSegments[seg].temp_C[i] = t;
    	    avgTemp += t;

    	    if (t > OT_THRESHOLD && t != -99.0f) {
    	    	PackSegments[seg].fault_flags |= 0x04;
    	    }
    	}
    }

    TotalPack.voltage = totalVoltage;
    TotalPack.avg_voltage = totalVoltage/(TOTAL_CELLS);

    //Check for minimum 20% of cells having temperature measurements, otherwise call a fault
	for (int mod = 0; mod < TOTAL_MODULES; mod++) {
		if (((float)(TOTAL_CELLS-dead_cells)/(float)TOTAL_CELLS)*0.5 < MIN_CELL_THRESH) {
			PackSegments[mod].fault_flags |= 0x10;
		} else {
			PackSegments[mod].fault_flags &= ~0x10;
		}
	}

	avgTemp = avgTemp/(TOTAL_CELLS);
	TotalPack.temp = avgTemp;
}

/* -------------------------------------------------------------------------- */
/* Real-Time Measurement Loop */
/* -------------------------------------------------------------------------- */
void measurement_loop(volatile CAN_Inputs_t *can_data) {

	//uint32_t old_basepri = __get_BASEPRI();
	//__set_BASEPRI(4 << (8 - __NVIC_PRIO_BITS));   // mask priorities 4..15

    // 1. Read Cell Voltages (Must Snap/Unsnap during CONTINUOUS mode!)
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_Snap(); // FREEZES CONTINUOUS IIR FILTER SO WE CAN READ

    // adBmsReadData OVERWRITES cccrc.cell_pec on every call, so without
    // accumulating here only the LAST group's (F) PEC survives. A corrupt
    // A-E read would then pass the comm-drop check below and get used as a
    // fake ~1.5-1.8V cell. OR the per-group PEC together so ANY bad group trips.
    uint8_t cell_pec_acc[TOTAL_IC];

    for (int i = 0; i < TOTAL_IC; i++) cell_pec_acc[i] = 0;

    adBmsReadData(TOTAL_IC, &IC[0], RDCVA, Cell, A);
    for (int i = 0; i < TOTAL_IC; i++) cell_pec_acc[i] |= IC[i].cccrc.cell_pec;
    adBmsReadData(TOTAL_IC, &IC[0], RDCVB, Cell, B);
    for (int i = 0; i < TOTAL_IC; i++) cell_pec_acc[i] |= IC[i].cccrc.cell_pec;
    adBmsReadData(TOTAL_IC, &IC[0], RDCVC, Cell, C);
    for (int i = 0; i < TOTAL_IC; i++) cell_pec_acc[i] |= IC[i].cccrc.cell_pec;
    adBmsReadData(TOTAL_IC, &IC[0], RDCVD, Cell, D);
    for (int i = 0; i < TOTAL_IC; i++) cell_pec_acc[i] |= IC[i].cccrc.cell_pec;
    adBmsReadData(TOTAL_IC, &IC[0], RDCVE, Cell, E);
    for (int i = 0; i < TOTAL_IC; i++) cell_pec_acc[i] |= IC[i].cccrc.cell_pec;
    adBmsReadData(TOTAL_IC, &IC[0], RDCVF, Cell, F);
    for (int i = 0; i < TOTAL_IC; i++) cell_pec_acc[i] |= IC[i].cccrc.cell_pec;

    // Write the accumulated PEC back so Process_Board_Data sees ANY corrupt group
    for (int i = 0; i < TOTAL_IC; i++) IC[i].cccrc.cell_pec = cell_pec_acc[i];

    adBms6830_Unsnap(); // UNFREEZES REGISTERS

    // 2. Trigger One-Shot GPIO Conversions
    adBms6830_Adax(AUX_OW_OFF, PUP_DOWN, AUX_ALL);
    Delay_ms(5); // Allow time for GPIO 1-5 Conversion

    // Same PEC-overwrite issue as the cell reads: accumulate across AUX groups
    // so a corrupt temperature read can't slip through the comm-drop check.
    uint8_t aux_pec_acc[TOTAL_IC];
    for (int i = 0; i < TOTAL_IC; i++) aux_pec_acc[i] = 0;

    adBmsReadData(TOTAL_IC, &IC[0], RDAUXA, Aux, A);
    for (int i = 0; i < TOTAL_IC; i++) aux_pec_acc[i] |= IC[i].cccrc.aux_pec;
    adBmsReadData(TOTAL_IC, &IC[0], RDAUXB, Aux, B);
    for (int i = 0; i < TOTAL_IC; i++) aux_pec_acc[i] |= IC[i].cccrc.aux_pec;
    adBmsReadData(TOTAL_IC, &IC[0], RDAUXC, Aux, C);
    for (int i = 0; i < TOTAL_IC; i++) aux_pec_acc[i] |= IC[i].cccrc.aux_pec;
    adBmsReadData(TOTAL_IC, &IC[0], RDAUXD, Aux, D);
    for (int i = 0; i < TOTAL_IC; i++) aux_pec_acc[i] |= IC[i].cccrc.aux_pec;

    for (int i = 0; i < TOTAL_IC; i++) IC[i].cccrc.aux_pec = aux_pec_acc[i];

    // 3. Trigger Redundant GPIO Conversions (GPIO 6-10)
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_Adax2(AUX_ALL);
    Delay_ms(5); // Allow time for GPIO 6-10 Conversion

    adBmsReadData(TOTAL_IC, &IC[0], RDRAXA, RAux, A);
    adBmsReadData(TOTAL_IC, &IC[0], RDRAXB, RAux, B);
    adBmsReadData(TOTAL_IC, &IC[0], RDRAXC, RAux, C);
    adBmsReadData(TOTAL_IC, &IC[0], RDRAXD, RAux, D);

    // 4. Process math & Output via Serial
    Process_Board_Data();

    if(can_data->balancing_enable){
    	Process_Cell_Balancing();
    }

    //Print_Status_To_Console();

    //__set_BASEPRI(old_basepri);   // restore at end

}

/* -------------------------------------------------------------------------- */
/* Main Initializer */
/* -------------------------------------------------------------------------- */
void adbms_main_init(volatile CAN_Inputs_t *can_data) {
    //printf("\r\n\r\nStarting ADBMS6830 Driver...\r\n");
    uint8_t pwma_tx_data[6] = {0x44, 0x44, 0x44, 0x44, 0x44, 0x44}; // Cells 1-8 at 50%
    uint8_t pwmb_tx_data[6] = {0x44, 0x44, 0x44, 0x44, 0x44, 0x44 }; // Cells 9-10 at 50%, 11-16 at 0%

    for(uint8_t cic = 0; cic < TOTAL_IC; cic++) {
        IC[cic].tx_cfga.refon = PWR_UP;
        IC[cic].tx_cfga.gpo = 0X3FF;

        //PWM activate for balancing
        memcpy(IC[cic].pwma.tx_data, pwma_tx_data, 6);
        memcpy(IC[cic].pwmb.tx_data, pwmb_tx_data, 6);

        // Matching your ADI application initialization perfectly:
        IC[cic].tx_cfgb.vov = SetOverVoltageThreshold(OV_THRESHOLD);
        IC[cic].tx_cfgb.vuv = SetUnderVoltageThreshold(UV_THRESHOLD);
    }

    // Write Configs
    adBmsWakeupIc(TOTAL_IC);
    adBmsWriteData(TOTAL_IC, &IC[0], WRCFGA, Config, A);
    adBmsWriteData(TOTAL_IC, &IC[0], WRCFGB, Config, B);

    adBmsWriteData(TOTAL_IC, &IC[0], WRPWM1, Pwm, A);
    adBmsWriteData(TOTAL_IC, &IC[0], WRPWM2, Pwm, B);
    // Send Continuous ADC Command (RD_ON like the ADI test code)
    adBmsWakeupIc(TOTAL_IC);
    adBms6830_Adcv(RD_ON, CONTINUOUS, DCP_OFF, RSTF_OFF, OW_OFF_ALL_CH);

    // Give ADC filter time to settle on the first set of readings
    Delay_ms(8);

    //Read all ADBMS6830 Chip IDs
    adBms6830_Snap();
    adBmsReadData(TOTAL_IC, &IC[0], RDSID, Sid, NONE);
    for(uint8_t cic = 0; cic < TOTAL_IC; cic++) {
    	uint64_t raw_id = (((uint64_t)IC[cic].sid.sid[5]) << 40U) | (((uint64_t)IC[cic].sid.sid[4]) << 32U) | (((uint64_t)IC[cic].sid.sid[3]) << 24U) | (((uint64_t)IC[cic].sid.sid[2]) << 16U) | (((uint64_t)IC[cic].sid.sid[1]) << 8U) | (uint64_t)IC[cic].sid.sid[0];
    	//Convert to 1-14 numbering system
    	uint8_t id = (uint8_t) lookupID(raw_id);
    	//printf("BOARD ID: 0x%08lX%08lX\r\n",
    	//       (unsigned long)(raw_id >> 32),
    	//       (unsigned long)(raw_id & 0xFFFFFFFF));
    	//printf("BOARD ID CONV: %d\r\n",id);
    	//ID assignment for top and bottom BMS
		if (id > 7){
    		PackSegments[cic].id[1] = id;
    	} else {
    		PackSegments[cic].id[0] = id;
    	}
    }
    adBms6830_Unsnap();
    measurement_loop(can_data);  // get real initial values into g_pack_voltage and g_avg_temp

    //Initialize SOC estimator
    adBms6830_soc_init();

    //printf("Configuration Written. Starting loop.\r\n");

}

void adBms_main_run(volatile CAN_Inputs_t *can_data, GPIO_Info_t *gpio_data)
{
	Update_Charger_Status();    		// sample PC6, true if charger plugged in
	measurement_loop(can_data);         // reads ADBMS, updates PackSegments
	Update_BMS_OK_Output(gpio_data);	// drives PC9 (BMS fault) based on PackSegments + charger
	adBms6830_soc_run(can_data);		// update SOC
}

//SOC Run, called in main BMS loop
void adBms6830_soc_run(volatile CAN_Inputs_t *can_data)
{
	float currCap = 0;
	float cap = 0;
	float uncertainty = 0;
	float minSoc = 2;
	for (int cic = 0; cic < TOTAL_MODULES; cic++) {
	    if (PackSegments[cic].fault_flags & 0x08) continue;

	    for (int i = 0; i < CELLS_PER_MOD; i++) {
	    	int temp_idx = cell_to_temp_index(i);
	        adBms6830_soc_update(cic, i,
	                            PackSegments[cic].cell_v_mV[i]/1000.0f,
	                            can_data->tractive_current,
	                            PackSegments[cic].temp_C[temp_idx]);
	        //Calculate overall pack information
	        if(g_soc_estimate[cic][i].soc < minSoc){
	        	minSoc = g_soc_estimate[cic][i].soc;
	        }
	        currCap += (g_soc_estimate[cic][i].capacity_ah)*(g_soc_estimate[cic][i].soc);
	        cap += g_soc_estimate[cic][i].capacity_ah;
	        uncertainty += g_soc_estimate[cic][i].confidence;
	    }

	}



	//TotalPack.soc = currCap/cap;
	TotalPack.soc = minSoc;
	TotalPack.capacity = cap;
	TotalPack.uncertainty = uncertainty/NUM_SERIES_CELLS;

	//Print SOC estimate every 10 loops
	/*
	static int soc_print_counter = 0;
	if (++soc_print_counter >= 10) {
		for (int cic = 0; cic < TOTAL_MODULES; cic++) {
			printf("\n--- Module %d SOC ---\n", cic + 1);
			for (int cell = 0; cell < CELLS_PER_MOD; cell++) {
				adBms6830_print_soc_estimate(cic, cell, &g_soc_estimate[cic][cell]);
			}
		}
		soc_print_counter = 0;
	}
	*/


	//Save to flash at large intervals
	static uint32_t last_save_tick = 0;
	uint32_t now = HAL_GetTick();
	if (now - last_save_tick >= 300000) {  // 5 minutes
		//adBms6830_soc_save_to_flash();
		last_save_tick = now;
	}



}

void adBms6830_soc_init(void)
{
	//Check battery lookup table, never should be a problem
    if (!BatteryModel_VerifyTable()) {
        //printf("ERROR: Battery lookup table invalid!\n");
        return;
    }

    //Loop through all cells
    for (int cic = 0; cic < TOTAL_MODULES; cic++) {
    	//Check if loss of comms on a Module
        if (PackSegments[cic].fault_flags & 0x08) {
            //printf("WARNING: Module %d offline during init, skipping\r\n", cic + 1);
            continue;
        }

        printf("Module %d: initializing\r\n", cic + 1);

        for (int i = 0; i < CELLS_PER_MOD; i++) {
            int temp_idx = cell_to_temp_index(i);

            KalmanSOC_PersistentState persistent;
            bool from_flash = SOC_Flash_Load(&persistent); // TODO: per-cell flash address when scaling

            //Use flash memory to initialize the SOC, otherwise use BMS measurements
            /*
            if (from_flash && KalmanSOC_InitFromFlash(&g_kalman_soc[cic][i], &persistent)) {
                //printf("  Cell %02d: restored from flash (SOC=%.1f%% SOH=%.1f%%)\r\n",
                //       i + 1, persistent.soc * 100.0f, persistent.soh_est * 100.0f);
                g_soc_initialized[cic][i] = true;
            } else {
            */
                KalmanSOC_InitFromVoltage(&g_kalman_soc[cic][i],
                                          PackSegments[cic].cell_v_mV[i]/1000.0f,
                                          PackSegments[cic].temp_C[temp_idx]);
                //printf("  Cell %d: init from voltage (V=%.3fV T=%.1fC SOC=%.1f%%)\r\n",
                //       i + 1, (float) PackSegments[cic].cell_v_mV[i]/1000.0f, (float) PackSegments[cic].temp_C[temp_idx],
                //       g_kalman_soc[cic][i].x.soc * 100.0f);
                g_soc_initialized[cic][i] = true;
            //}

            //Initialize RC lookup tables from above
            KalmanSOC_SetRCLookupTables(&g_kalman_soc[cic][i],
                                        &g_r_ct_lut, &g_c_ct_lut,
                                        &g_r_dif_lut, &g_c_dif_lut,&g_r0_lut);
            g_soc_update_counter[cic][i] = 0;
        }
    }
}


// SOC CHANGES @brief Update SOC estimate with latest measurements

void adBms6830_soc_update(int cic, int cell, float cell_voltage, float pack_current, float cell_temp)
{
	//Check for initialization
    if (!g_soc_initialized[cic][cell]) return;

    //Update measurement storage from BMS readings and Inverter/Charger Current
    SOC_Measurement meas;
    meas.voltage      = cell_voltage;
    meas.current      = pack_current;
    meas.temperature  = cell_temp;
    meas.timestamp_ms = HAL_GetTick();

    //Call the update
    bool success = KalmanSOC_Update(&g_kalman_soc[cic][cell], &meas, &g_soc_estimate[cic][cell]);

    if (success) {
        g_soc_update_counter[cic][cell]++;
        //Every 100 SOC updates, save to flash memory
        /*
        if (g_soc_update_counter[cic][cell] >= 100) {
            KalmanSOC_PersistentState persistent;
            KalmanSOC_GetPersistentState(&g_kalman_soc[cic][cell], &persistent);
            bool flash_ok = SOC_Flash_Save(&persistent); // TODO: unique address per cic/cell when scaling
            if (flash_ok)
                //printf("Module %d Cell %02d: SOC saved (SOC=%.1f%% SOH=%.1f%%)\r\n",
                //       cic + 1, cell + 1,
                //       persistent.soc * 100.0f, persistent.soh_est * 100.0f);
            else
                //printf("WARNING: Module %d Cell %02d flash save failed\r\n", cic + 1, cell + 1);
            g_soc_update_counter[cic][cell] = 0;
        }
        */
    }
}


// SOC CHANGES @brief Print SOC estimate to console
void adBms6830_print_soc_estimate(int cic, int cell, const SOC_Estimate *est)
{
    if (!est->valid) return;
    printf(" Cell %02d: SOC=%5.1f%%  OCV=%.3fV  R0=%.4f  Cap=%.2fAh  SOH=%5.1f%%\r\n",
           cell + 1, est->soc_percent, est->voltage_ocv,
           est->r0, est->capacity_ah, est->soh_percent);
}


//STORE IN FLASH MEMORY
void adBms6830_soc_save_to_flash(void)
{
    for (int cic = 0; cic < TOTAL_MODULES; cic++) {
        for (int cell = 0; cell < CELLS_PER_MOD; cell++) {
            if (!g_soc_initialized[cic][cell]) continue;
            KalmanSOC_PersistentState persistent;
            KalmanSOC_GetPersistentState(&g_kalman_soc[cic][cell], &persistent);
            bool success = SOC_Flash_Save(&persistent); // TODO: unique address per cell
            /*
            if (success)
                printf("Module %d Cell %02d: saved (SOC=%.1f%% SOH=%.1f%%)\r\n",
                       cic + 1, cell + 1,
                       persistent.soc * 100.0f, persistent.soh_est * 100.0f);
            else
                printf("ERROR: Module %d Cell %02d flash save failed\r\n", cic + 1, cell + 1);
                */
        }
    }
}


//SOC CHANGES @brief Clear flash storage (for testing)
void adBms6830_soc_clear_flash(void)
{
    bool success = SOC_Flash_Erase();
    
    if (success) {
        //printf("Flash storage cleared\n");
    } else {
        //printf("ERROR: Flash erase failed\n");
    }

}

//Cell Index conversion to temperature index (24 -> 20 in the case of the full module)
int cell_to_temp_index(int cell_i) {
	if (cell_i <= 11)  return cell_i;
	if (cell_i <= 13)  return 12;
    if (cell_i <= 15)  return 13;
    if (cell_i <= 17)  return 14;
    if (cell_i <= 19)  return 15;
    if (cell_i >= 20) return cell_i-4;
    return cell_i;
}

//ADBMS6830 ID Lookup Conversion
int lookupID(uint64_t id){
	if (id == (uint64_t) ID1_TOP) return 1;
	if (id == (uint64_t) ID2_TOP) return 2;
	if (id == (uint64_t) ID3_TOP) return 3;
	if (id == (uint64_t) ID4_TOP) return 4;
	if (id == (uint64_t) ID5_TOP) return 5;
	if (id == (uint64_t) ID6_TOP) return 6;
	if (id == (uint64_t) ID7_TOP) return 7;
	if (id == (uint64_t) ID1_BOTTOM) return 8;
	if (id == (uint64_t) ID2_BOTTOM) return 9;
	if (id == (uint64_t) ID3_BOTTOM) return 10;
	if (id == (uint64_t) ID4_BOTTOM) return 11;
	if (id == (uint64_t) ID5_BOTTOM) return 12;
	if (id == (uint64_t) ID6_BOTTOM) return 13;
	if (id == (uint64_t) ID7_BOTTOM) return 14;
	return 0;
}

#include "can_management.h"
#include "adBms_Application.h"
#include "common.h"
#include <math.h>
#include <stdint.h>

static uint8_t RxData[8];
volatile CAN_Inputs_t can_data = {0};
static FDCAN_RxHeaderTypeDef RxHeader;
Errors error_info = {0};

void can_init(FDCAN_HandleTypeDef *hfdcan1,GPIO_Info_t *gpio)
{
	//Switch CAN Baud rate based on what the battery is plugged in to
	if(gpio->slow_CAN){ // Charger
		hfdcan1->Init.NominalPrescaler = CAN_Charger_Nominal_Prescaler;
	    hfdcan1->Init.NominalSyncJumpWidth = CAN_Charger_Nominal_SJW;
		hfdcan1->Init.NominalTimeSeg1 = CAN_Charger_Nominal_TSeg_1;
	    hfdcan1->Init.NominalTimeSeg2 = CAN_Charger_Nominal_TSeg_2;
		hfdcan1->Init.DataPrescaler = CAN_Charger_Data_Prescaler;
		hfdcan1->Init.DataSyncJumpWidth = CAN_Charger_Data_SJW;
		hfdcan1->Init.DataTimeSeg1 = CAN_Charger_Data_TSeg_1;
		hfdcan1->Init.DataTimeSeg2 = CAN_Charger_Data_TSeg_2;
	} else { // Car
		hfdcan1->Init.NominalPrescaler = CAN_CAR_Nominal_Prescaler;
		hfdcan1->Init.NominalSyncJumpWidth = CAN_CAR_Nominal_SJW;
		hfdcan1->Init.NominalTimeSeg1 = CAN_CAR_Nominal_TSeg_1;
		hfdcan1->Init.NominalTimeSeg2 = CAN_CAR_Nominal_TSeg_2;
		hfdcan1->Init.DataPrescaler = CAN_CAR_Data_Prescaler;
		hfdcan1->Init.DataSyncJumpWidth = CAN_CAR_Data_SJW;
		hfdcan1->Init.DataTimeSeg1 = CAN_CAR_Data_TSeg_1;
		hfdcan1->Init.DataTimeSeg2 = CAN_CAR_Data_TSeg_2;
	}

	//Re-Initialize FDCAN1
	if (HAL_FDCAN_Init(hfdcan1) != HAL_OK)
	{
	     Error_Handler();
		 return;
	}

	//Start FDCAN1
	 if (HAL_FDCAN_Start(hfdcan1) != HAL_OK){
		 Error_Handler();
		 return;

	 }

	 //Activate the Notification for new data in FIFO0 for FDCAN1
	 if (HAL_FDCAN_ActivateNotification(hfdcan1,FDCAN_IT_RX_FIFO0_NEW_MESSAGE,0) != HAL_OK){
		 Error_Handler();
		 return;
	 }
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs){
	if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET){
		//Retrieve Rx Messages from Rx FIFO0
		//printf("CAN message receive\r\n");
		//while(HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0){
		if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK){
			// Reception Error
			//printf("CAN GET Error\r\n");
			error_info.message_receive_errors |= (1 << CAN_RECEPTION_ERROR);
			return;
		} else {
			error_info.message_receive_errors &= ~(1 << CAN_RECEPTION_ERROR);
		}

		//Switch Statement that reads CAN message ID to fill in correct information
		switch (RxHeader.Identifier){
			case CAN_ID_CHARGER:
				can_data.balancing_enable = (RxData[0] & 0b1);
				//can_data.tractive_current = (float)((((uint16_t)RxData[2]) << 8) | ((uint16_t)RxData[1])); //TODO: CHANGE
				break;
			case CAN_ID_INVERTER_CURRENT:
				can_data.tractive_current = (float)((int16_t)((((uint16_t)RxData[7]) << 8) | ((uint16_t)RxData[6])))*0.1f;
				break;
			case CAN_ID_INVERTER_VOLTAGE:
				can_data.dc_bus_voltage = (float)((((uint16_t)RxData[1]) << 8) | ((uint16_t)RxData[0]))*0.1f;
				break;
			case CAN_ID_ELCON_CURRENT:
				can_data.tractive_current = (float)((int16_t)((((uint16_t)RxData[2]) << 8) | ((uint16_t)RxData[3])))*0.1f;
				break;
			default:
				break;
		}
		//}
	}
}

void bms_can_faults(SegmentData_t *PackData,TotalPack_t *TotalPack,FDCAN_HandleTypeDef *hfdcan1){
	//Send total voltage and all BMS fault flags for all Modules
	uint8_t data1[8] = {0};
	uint16_t temp_totalVoltage = clamp_u16((TotalPack->voltage)/0.01f,0.0f,65535.0f);
	data1[0] = (uint8_t)(temp_totalVoltage & 0xFF);
	data1[1] = (uint8_t)(temp_totalVoltage >> 8U);
	data1[2] = PackData[0].fault_flags | (TotalPack->balancing_done << 8U);
	for(int cic = 1; cic < TOTAL_MODULES; cic++){
		data1[cic+2] = PackData[cic].fault_flags;
	}
	if (CAN_SendData((uint16_t) CAN_ID_BMS_PACK, data1, FDCAN_DLC_BYTES_8,hfdcan1) != HAL_OK){
		//printf("CAN Error BMS Pack\r\n");
		error_info.message_send_errors |= (1 << BMS_CAN_PACK_ERROR);
	} else {
		error_info.message_send_errors &= ~(1 << BMS_CAN_PACK_ERROR);
	}
}

void bms_can_stats(SegmentData_t *PackData,TotalPack_t *TotalPack,FDCAN_HandleTypeDef *hfdcan1){
	uint8_t maxVoltageIdx = 0;
	uint8_t minVoltageIdx = 0;
	uint8_t maxTempIdx = 0;
	uint8_t minTempIdx = 0;
	uint32_t maxVoltage = 0;
	uint32_t minVoltage = 65535;
	int8_t maxTemp = -128;
	int8_t minTemp = 127;

	//Calculate min, max, temp and voltage and total voltage from BMS information
	for(int cic = 0; cic < TOTAL_MODULES; cic++) {
		for(int i = 0; i < CELLS_PER_MOD; i++){
			uint32_t temp_idx = cell_to_temp_index(i);
			if(PackData[cic].cell_v_mV[i] > maxVoltage){
				maxVoltage = PackData[cic].cell_v_mV[i];
				maxVoltageIdx = cic * CELLS_PER_MOD + i;
			}
			if(PackData[cic].cell_v_mV[i] < minVoltage){
				minVoltage = PackData[cic].cell_v_mV[i];
				minVoltageIdx = cic * CELLS_PER_MOD + i;
			}
			if(PackData[cic].temp_C[temp_idx] > maxTemp){
				maxTemp = PackData[cic].temp_C[temp_idx];
				maxTempIdx = cic * CELLS_PER_MOD + i;
			}
			if(PackData[cic].temp_C[temp_idx] < minTemp){
				minTemp = PackData[cic].temp_C[temp_idx];
				minTempIdx = cic * CELLS_PER_MOD + i;
			}
		}
	}
	//Send min and max voltage and temp values and IDs in structure
	uint8_t data2[8];
	data2[0] = (uint8_t)clamp_u8((((maxVoltage/1000.0f)-1.8)/0.01),0.0f,255.0f);
	data2[1] = (uint8_t)clamp_u8((((minVoltage/1000.0f)-1.8)/0.01),0.0f,255.0f);
	data2[2] = (uint8_t)clamp_u8(maxTemp*4.0f,0.0f,255.0f);
	data2[3] = (uint8_t)clamp_u8(minTemp*4.0f,0.0f,255.0f);
	data2[4] = maxVoltageIdx;
	data2[5] = minVoltageIdx;
	data2[6] = maxTempIdx;
	data2[7] = minTempIdx;
	if (CAN_SendData((uint16_t) CAN_ID_BMS_STATS, data2, FDCAN_DLC_BYTES_8,hfdcan1) != HAL_OK){
		//printf("CAN Error BMS Stats\r\n");
		error_info.message_send_errors |= (1 << BMS_CAN_STATS_ERROR);
	} else {
		error_info.message_send_errors &= ~(1 << BMS_CAN_STATS_ERROR);
	}
}

void bms_can_data(SegmentData_t *PackData,TotalPack_t *TotalPack,FDCAN_HandleTypeDef *hfdcan1, uint8_t *bms_mod_counter, uint8_t *bms_segment_counter){
	//BMS CAN ID conversion based on which Module and starting Cell is given to the function
	uint16_t id = CAN_ID_BMS_INIT + ((*bms_segment_counter)*6) + ((*bms_mod_counter+1)/4);
	uint8_t data[8];

	//Voltage and Temperature Assignment
	for(int i = 0; i<4; i++){
		//Assign voltages and temperatures
		uint32_t temp_idx = cell_to_temp_index(*bms_mod_counter); //Special conversion with offset num of temp sensors and voltage sensors
		data[i] = clamp_u8((((PackData[*bms_segment_counter].cell_v_mV[*bms_mod_counter])/(1000.0f)-1.8)/0.01), 0.0f, 255.0f);
		data[i+4] = clamp_u8(PackData[*bms_segment_counter].temp_C[temp_idx]*4.0f, 0.0f, 255.0f);
		*bms_mod_counter += 1;
		//Check for end of module
		if(*bms_mod_counter >= (uint8_t)CELLS_PER_MOD){
			*bms_segment_counter += 1;
			//Check for final Module
			if(*bms_segment_counter == TOTAL_MODULES){
				*bms_segment_counter = 0;
			}
			*bms_mod_counter = 0;
			//Fill in rest of the data with zeros
			for(int j=i+1; j<4; j++){
				data[j] = clamp_u8((uint8_t)0.0f,0.0f,255.0f);
				data[j+4] = clamp_u8((uint8_t)0.0f,0.0f,255.0f);
			}
			break;
		}
	}
	if (CAN_SendData(id, data, FDCAN_DLC_BYTES_8, hfdcan1) != HAL_OK){
		//printf("CAN Error BMS = %d\r\n",id);
		error_info.message_send_errors |= (1 << BMS_CAN_ERROR);
	} else {
		//printf("BMS Send ID = %d\r\n",id);
		error_info.message_send_errors &= ~(1 << BMS_CAN_ERROR);
	}
}

void bsm_can(bsm_obj *bsmInfo,GPIO_Info_t *gpio_data, FDCAN_HandleTypeDef *hfdcan1)
{
	//Send battery state machine outputs and state
	uint8_t data[5];
	data[0] = (int8_t) bsmInfo->state;
	data[1] = bsmInfo->pc_enable;
	data[2] = bsmInfo->ir_plus_enable;
	data[3] = bsmInfo->ir_minus_enable;
	data[5] = gpio_data->ir_plus_aux;
	data[6] = gpio_data->ir_minus_aux;
	if (bsmInfo->state == FAULT){
		data[4] = (uint8_t) 1;
	} else {
		data[4] = 0;
 	}
	if (CAN_SendData((uint16_t) CAN_ID_BSM, data,FDCAN_DLC_BYTES_7,hfdcan1) != HAL_OK){
		//printf("CAN Error BSM\r\n");
		error_info.message_send_errors |= (1 << BSM_CAN_ERROR);
		 //return;
	} else {
		error_info.message_send_errors &= ~(1 << BSM_CAN_ERROR);
	}

}
void bms_can_ids(SegmentData_t *PackData, SOC_Estimate soc[][CELLS_PER_MOD],TotalPack_t *pack,FDCAN_HandleTypeDef *hfdcan1){
	//Send individual BMS board IDs
	uint8_t data[8] = {0};
	for(int cic = 0; cic < TOTAL_MODULES; cic++) {
		data[cic] = (PackData[cic].id[1] << 4U) | (PackData[cic].id[0]);
	}

	data[6] =  clamp_u8(pack->temp*4.0f, 0.0f, 255.0f);
	data[7] = clamp_u8(((pack->avg_voltage-1.8)/0.01), 0.0f, 255.0f);

	if (CAN_SendData((uint16_t) CAN_ID_BMS_IDS, data, FDCAN_DLC_BYTES_8,hfdcan1) != HAL_OK){
		//printf("CAN Error BMS IDS\r\n");
		error_info.message_send_errors |= (1 << BMS_CAN_IDS_ERROR);
			//return;
	} else {
		//printf("CAN NO ERROR BMS IDS\r\n");
		error_info.message_send_errors &= ~(1 << BMS_CAN_IDS_ERROR);
	}
}

void soc_can_stats(SegmentData_t *PackData, SOC_Estimate soc[][CELLS_PER_MOD],TotalPack_t *pack,FDCAN_HandleTypeDef *hfdcan1){
	//Overall Pack SOC Information
	uint8_t data_header[6];
	float clamped_cap = fmaxf(fminf(10.4*144,pack->capacity),0);
	clamped_cap = clamped_cap/(10.4*144);
	data_header[0] = (uint8_t)((uint16_t)(pack->soc * 65535.0f) & 0xFF);
	data_header[1] = (uint8_t)(((uint16_t)(pack->soc * 65535.0f)) >> 8U);
	data_header[2] = (uint8_t)((uint16_t)(clamped_cap * 65535.0f) & 0xFF);
	data_header[3] = (uint8_t)(((uint16_t)(clamped_cap * 65535.0f)) >> 8U);
	data_header[4] = (uint8_t)((uint16_t)(pack->uncertainty * 65535.0f) & 0xFF);
	data_header[5] = (uint8_t)(((uint16_t)(pack->uncertainty * 65535.0f)) >> 8U);

	if (CAN_SendData(CAN_ID_SOC_INIT,data_header,FDCAN_DLC_BYTES_6,hfdcan1) != HAL_OK){
		//printf("CAN Error SOC Pack\r\n");
		error_info.message_send_errors |= (1 << SOC_CAN_PACK_ERROR);
	} else {
		//printf("SOC PACK Error Send\r\n");
		error_info.message_send_errors &= ~(1 << SOC_CAN_PACK_ERROR);
	}
}

void soc_can_data(SOC_Estimate soc[][CELLS_PER_MOD],TotalPack_t *pack,FDCAN_HandleTypeDef *hfdcan1,uint8_t *soc_mod_counter,uint8_t *soc_segment_counter){
	//SOC CAN ID conversion based on which Module and starting Cell is given to the function
	uint16_t id = CAN_ID_SOC_INIT+ 1 + ((*soc_segment_counter)*3) + ((*soc_mod_counter+1)/8);
	uint8_t data[8];
	for(int i = 0; i<8; i++){
		//SOC Assignment
		data[i] = (uint8_t)(soc[*soc_segment_counter][*soc_mod_counter].soc*255.0f);
		*soc_mod_counter += 1;
		//Check for end of Module
		if((*soc_mod_counter+1) % ((uint8_t)CELLS_PER_MOD+1) == 0){
			*soc_segment_counter += 1;
			//Check for last module
			if(*soc_segment_counter == TOTAL_MODULES){
				*soc_segment_counter = 0;
			}
			*soc_mod_counter = 0;
			//Fill in rest of the data with zeros
			for(int j = i+1; j<8; j++){
				data[j] = clamp_u8((uint8_t)0.0f,0.0f,255.0f);
			}
			break;
		}
	}
	if (CAN_SendData(id,data,FDCAN_DLC_BYTES_8,hfdcan1) != HAL_OK){
		//printf("CAN Error SOC = %d\r\n", id);
		error_info.message_send_errors |= (1 << SOC_CAN_ERROR);
	} else {
		//printf("SOC ID Send = %d\r\n",id);
		error_info.message_send_errors &= ~(1 << SOC_CAN_ERROR);
	}
}

void error_can(FDCAN_HandleTypeDef *hfdcan1){
	//Send error bits
	uint8_t data[4];
	data[0] = error_info.message_send_errors;
	data[1] = error_info.message_receive_errors;
	data[2] = error_info.message_init_send_errors;
	data[3] = 1; //Message existing
	if(CAN_SendData((uint16_t) CAN_ID_ERRORS,data,FDCAN_DLC_BYTES_4,hfdcan1) != HAL_OK){
		//printf("CAN Error Error\r\n");
	} else {
		//printf("CAN NOT Error Error\r\n");
	}
}

void adc_can(ADC_Inputs_t *adc_data, FDCAN_HandleTypeDef *hfdcan1){
	//Convert ADCs and send
	uint8_t data[7];
	uint16_t ts12 = clamp_u16(adc_data->ts_vsense / PACK_VOLTAGE_SCALE, 0, 4095);
	uint16_t bat12 = clamp_u16(adc_data->bat_vsense / PACK_VOLTAGE_SCALE, 0, 4095);
	data[0] = ts12 & 0xFF;
	data[1] = ((ts12 >> 8) & 0x0F) | ((bat12 << 4) & 0xF0);
	data[2] = (bat12 >> 4) & 0xFF;
	data[3] = clamp_u8((adc_data->temp_precharge - TSENSE_OFFSET) / TSENSE_SCALE, 0.0f, 255.0f);
	data[4] = clamp_u8((adc_data->temp_power - TSENSE_OFFSET) / TSENSE_SCALE, 0.0f, 255.0f);
	data[5] = clamp_u8((adc_data->temp_vsense - TSENSE_OFFSET) / TSENSE_SCALE, 0.0f, 255.0f);
	data[6] = clamp_u8((adc_data->temp_ambient - TSENSE_OFFSET) / TSENSE_SCALE, 0.0f, 255.0f);
	HAL_StatusTypeDef temp = CAN_SendData((uint16_t) CAN_ID_PACK_SENSE,data,FDCAN_DLC_BYTES_7,hfdcan1);
	if(temp != HAL_OK){
		//printf("CAN Error ADC %d\r\n",temp);
		error_info.message_send_errors |= (1 << PACK_SENSE_ERROR);
	} else {
		//printf("CAN NOT Error ADC\r\n");
		error_info.message_send_errors &= ~(1 << PACK_SENSE_ERROR);
	}
}

HAL_StatusTypeDef CAN_SendData(uint16_t id, uint8_t *data, uint32_t length,FDCAN_HandleTypeDef *hfdcan1)
{
	if (HAL_FDCAN_GetTxFifoFreeLevel(hfdcan1) == 0) {
	    return HAL_BUSY;
	}

	//Set up the transmit header with the proper ID and length
    FDCAN_TxHeaderTypeDef txHdr;
    txHdr.Identifier        = id;
    txHdr.IdType            = FDCAN_STANDARD_ID;
    txHdr.TxFrameType       = FDCAN_DATA_FRAME;
    txHdr.DataLength        = length;
    txHdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHdr.BitRateSwitch     = FDCAN_BRS_OFF;
    txHdr.FDFormat          = FDCAN_CLASSIC_CAN;  // match your init FrameFormat
    txHdr.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    txHdr.MessageMarker     = 0;

    return HAL_FDCAN_AddMessageToTxFifoQ(hfdcan1, &txHdr, data);
}

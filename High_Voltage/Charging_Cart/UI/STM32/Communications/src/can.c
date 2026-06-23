#include "can.h"

#include "main.h"
#include "chargersm.h"
#include "stm32f0xx_hal.h"


static uint8_t RxData[8];
static CAN_RxHeaderTypeDef RxHeader;
SegmentData_t bms_data[TOTAL_SEGMENTS];
Errors CAN_error_info = {0};
CANInfo CAN_Info = {0};

void cell_data_parsing(uint32_t id, SegmentData_t *bms_data, uint8_t *rx_data){
	uint8_t bms_module = (id - CAN_ID_BMS_DATA_START)/6;
	uint8_t cell_start_index = ((id - CAN_ID_BMS_DATA_START) % 6)*4;
	for (uint8_t i = 0; i < 4; i++){
		bms_data[bms_module].cell_v[cell_start_index + i] = rx_data[i];
		bms_data[bms_module].temp_C[cell_start_index + i] = rx_data[i+4];
	}
}

void cell_soc_parsing(uint32_t id, SegmentData_t *bms_data, uint8_t *rx_data){
	uint8_t bms_module = (id - CAN_ID_SOC_DATA_START)/3;
	uint8_t cell_start_index = ((id - CAN_ID_SOC_DATA_START) % 3)*8;
	for (uint8_t i = 0; i < 8; i++){
		bms_data[bms_module].soc[cell_start_index + i] = rx_data[i];
	}
}

void elcon_to_charger_parsing(uint8_t *rx_data, uint8_t dlc){
	if (dlc < 5) return; // Not enough data for voltage, current, and status

	CAN_Info.elconVoltage = (((uint16_t)rx_data[0]) << 8) | rx_data[1];
	CAN_Info.elconCurrent = (((uint16_t)rx_data[2]) << 8) | rx_data[3];
	CAN_Info.elconStatus = rx_data[4];
}

void can_init(CAN_HandleTypeDef *hdcan)
{
	HAL_GPIO_WritePin(CAN_SLEEP_GPIO_Port, CAN_SLEEP_Pin, GPIO_PIN_RESET); //enable CAN transceiver
	//Start FDCAN1
	if (HAL_CAN_Start(hdcan) != HAL_OK){
		Error_Handler();
	}

	 //Activate the Notification for new data in FIFO0 for FDCAN1
	 if (HAL_CAN_ActivateNotification(hdcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK){
		CAN_error_info.message_receive_errors |= (1 << CAN_NOTIFICATION_ERROR);
		return;
	}
	CAN_error_info.message_receive_errors &= ~(1 << CAN_NOTIFICATION_ERROR);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan){
	// Retrieve Rx Messages from Rx FIFO0
	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK){
		CAN_error_info.message_receive_errors |= (1 << CAN_RECEPTION_ERROR);
		return;
	}
	CAN_error_info.message_receive_errors &= ~(1 << CAN_RECEPTION_ERROR);

	
	uint32_t id = (RxHeader.IDE == CAN_ID_EXT) ? RxHeader.ExtId : RxHeader.StdId;
	
	if (id == CAN_ID_BATTERY_PACK) { //Node ID
		CAN_Info.packVoltage =  (((uint16_t)RxData[1]) << 8) | (RxData[0]);
		CAN_Info.balancingDone =  RxData[2];
		CAN_Info.nomVoltVal = (uint8_t)((CAN_Info.packVoltage / 60) - 180);
	} else if (id == CAN_ID_BSM) {
		CAN_Info.bsmState = (int8_t) RxData[0];
		CAN_Info.lastBSMUpdateTick = HAL_GetTick();
	} else if (id == CAN_ID_SOC_PACK) {
		CAN_Info.SOC = (((uint16_t)RxData[1]) << 8) | (RxData[0]);
	} else if (id == CAN_ID_BMS_STATS) {
		CAN_Info.maxVoltVal  = RxData[0];
		CAN_Info.minVoltVal  = RxData[1];
		CAN_Info.maxTempVal  = RxData[2];
		CAN_Info.minTempVal  = RxData[3];
		CAN_Info.maxVoltCell = RxData[4];
		CAN_Info.minVoltCell = RxData[5];
		CAN_Info.maxTempCell = RxData[6];
		CAN_Info.minTempCell = RxData[7];
	} else if (id == CAN_ID_BMS_AVGS) {
		CAN_Info.nomTempVal  = RxData[6];
		//CAN_Info.nomVoltVal  = RxData[7];
	} else if (CAN_ID_BMS_DATA_START <= id && id <= CAN_ID_BMS_DATA_END) {
		cell_data_parsing(id, bms_data, RxData);
	} else if (CAN_ID_SOC_DATA_START <= id && id <= CAN_ID_SOC_DATA_END) {
		cell_soc_parsing(id, bms_data, RxData);
	} else if (id == CAN_ID_ELCON_TO_CHARGING) {
		elcon_to_charger_parsing(RxData, RxHeader.DLC);
		CAN_Info.lastElconUpdateTick = HAL_GetTick();
	}

}

bool battery_can(CAN_HandleTypeDef *hcan, uint16_t current, bool balancing){

	uint8_t data[3];
	data[0] = balancing;
	data[1] = (uint8_t)(current >> 8);
	data[2] = (uint8_t)(current & 0xFF);

	if(CAN_SendData(hcan, CAN_CHARGING_TO_PACK,data,3,false) != HAL_OK){
		CAN_error_info.message_send_errors |= (1 << CHARGER_TO_PACK_ERROR);
		return false;
	}
	CAN_error_info.message_send_errors &= ~(1 << CHARGER_TO_PACK_ERROR);
	return true;
}

bool error_can(CAN_HandleTypeDef *hcan){
	uint8_t data[3];
	data[0] = CAN_error_info.message_send_errors;
	data[1] = CAN_error_info.message_receive_errors;
	data[2] = CAN_error_info.message_init_send_errors;
	
	return CAN_SendData(hcan, CAN_ID_CHARGER_ERRORS,data,3,false) == HAL_OK;
}

//NOTE: v_lim and i_lim are in deci-units (e.g., 412 means 41.2V or A)
bool charger_can(CAN_HandleTypeDef *hcan, uint16_t v_lim, uint16_t i_lim, bool charge){
	uint8_t data[8];
	data[0] = (uint8_t)(v_lim >> 8);
	data[1] = (uint8_t)(v_lim & 0xFF);

	data[2] = (uint8_t)(i_lim >> 8);
	data[3] = (uint8_t)(i_lim & 0xFF);

	//note that 0 means charge and 1 means don't charge
	data[4] = charge ? 0 : 1;

	return CAN_SendData(hcan, CAN_ID_CHARGING_TO_ELCON,data,8,true) == HAL_OK;
}


HAL_StatusTypeDef CAN_SendData(CAN_HandleTypeDef *hcan, uint32_t id, uint8_t *data, uint32_t length, bool ext)
{
	uint32_t TxMailbox;
    CAN_TxHeaderTypeDef txHdr;
	txHdr.DLC = length;
	txHdr.RTR = CAN_RTR_DATA;
	if (ext){
		txHdr.IDE = CAN_ID_EXT;
		txHdr.ExtId = id;
	} else {
		txHdr.IDE = CAN_ID_STD;
		txHdr.StdId = id;
	}
	
    return HAL_CAN_AddTxMessage(hcan, &txHdr, data, &TxMailbox);
}

//Cell conversion functions

//output in mV
uint16_t cell_voltage_conversion(uint8_t raw_value) {
	return ((uint16_t)raw_value) * 10 + 1800;
}

//output in 0.01C
uint16_t cell_temperature_conversion(int8_t raw_value) {
	return ((uint16_t)raw_value) * 25;
}

//output in percentage
uint16_t cell_soc_conversion(uint8_t raw_value) {
	return ((uint16_t)raw_value*100)/255;
}

//output in 0.1V
uint16_t pack_voltage_conversion(uint16_t raw_value) {
	return raw_value/10;
}

//output in percentage
uint16_t pack_soc_conversion(uint16_t raw_value){
	return (uint16_t)(((uint32_t)raw_value*100)/65535);
}
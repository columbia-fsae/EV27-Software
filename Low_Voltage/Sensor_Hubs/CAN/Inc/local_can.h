#ifndef LOCAL_CAN_H
#define LOCAL_CAN_H

#include <stdbool.h>
#include <stdint.h>

#include "Common.h"
#include "global_can.h"

#define CAN_ID_TEMP_FL_IN1 ((uint16_t)0x4b0)
#define CAN_ID_TEMP_FL_IN2 ((uint16_t)0x4b2)
#define CAN_ID_TEMP_FL_IN3 ((uint16_t)0x4b2)
#define CAN_ID_TEMP_FL_IN4 ((uint16_t)0x4b3)
#define CAN_ID_TEMP_FR_IN1 ((uint16_t)0x4b4)
#define CAN_ID_TEMP_FR_IN2 ((uint16_t)0x4b5)
#define CAN_ID_TEMP_FR_IN3 ((uint16_t)0x4b6)
#define CAN_ID_TEMP_FR_IN4 ((uint16_t)0x4b7)
#define CAN_ID_TEMP_RL_IN1 ((uint16_t)0x4b8)
#define CAN_ID_TEMP_RL_IN2 ((uint16_t)0x4b9)
#define CAN_ID_TEMP_RL_IN3 ((uint16_t)0x4ba)
#define CAN_ID_TEMP_RL_IN4 ((uint16_t)0x4bb)
#define CAN_ID_TEMP_RR_IN1 ((uint16_t)0x4bc)
#define CAN_ID_TEMP_RR_IN2 ((uint16_t)0x4bd)
#define CAN_ID_TEMP_RR_IN3 ((uint16_t)0x4be)
#define CAN_ID_TEMP_RR_IN4 ((uint16_t)0x4bf)

typedef struct {
    uint32_t id;
    uint8_t data[8];
    uint32_t data_length;
} can_receive_msg;

#endif

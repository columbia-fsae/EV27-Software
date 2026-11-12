#ifndef LOCAL_CAN_H
#define LOCAL_CAN_H

#include <stdbool.h>
#include <stdint.h>

#include "Common.h"
#include "global_can.h"

typedef struct {
    uint32_t id;
    uint8_t data[8];
    uint32_t data_length;
} can_receive_msg;

extern struct temp temp_data[16];
#endif

#include "fake_can_management.h"

#include <string.h>

FAKE_CAN_MANAGEMENT_LIST(DEFINE_FAKE_VALUE_FUNC, DEFINE_FAKE_VOID_FUNC)

// Globals owned by the real can_management.c
volatile CAN_Inputs_t can_data;
Errors error_info;

#define RESET_VALUE_FAKE(rtype, name, ...) RESET_FAKE(name);
#define RESET_VOID_FAKE(name, ...)         RESET_FAKE(name);

void fake_can_management_reset(void) {
    FAKE_CAN_MANAGEMENT_LIST(RESET_VALUE_FAKE, RESET_VOID_FAKE)
    memset((void*)&can_data, 0, sizeof(can_data));
    memset(&error_info, 0, sizeof(error_info));
}

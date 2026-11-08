#include "fake_battery_model.h"

FAKE_BATTERY_MODEL_LIST(DEFINE_FAKE_VALUE_FUNC, DEFINE_FAKE_VOID_FUNC)

#define RESET_VALUE_FAKE(rtype, name, ...) RESET_FAKE(name);
#define RESET_VOID_FAKE(name, ...)         RESET_FAKE(name);

void fake_battery_model_reset(void) { FAKE_BATTERY_MODEL_LIST(RESET_VALUE_FAKE, RESET_VOID_FAKE) }

#include "fake_adbms_driver.h"

FAKE_ADBMS_DRIVER_LIST(DEFINE_FAKE_VALUE_FUNC, DEFINE_FAKE_VOID_FUNC)

#define RESET_VALUE_FAKE(rtype, name, ...) RESET_FAKE(name);
#define RESET_VOID_FAKE(name, ...)         RESET_FAKE(name);

void fake_adbms_driver_reset(void) { FAKE_ADBMS_DRIVER_LIST(RESET_VALUE_FAKE, RESET_VOID_FAKE) }

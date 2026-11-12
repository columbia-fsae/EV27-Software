#include "fake_kalman_soc.h"

FAKE_KALMAN_SOC_LIST(DEFINE_FAKE_VALUE_FUNC, DEFINE_FAKE_VOID_FUNC)

#define RESET_VALUE_FAKE(rtype, name, ...) RESET_FAKE(name);
#define RESET_VOID_FAKE(name, ...)         RESET_FAKE(name);

void fake_kalman_soc_reset(void) { FAKE_KALMAN_SOC_LIST(RESET_VALUE_FAKE, RESET_VOID_FAKE) }

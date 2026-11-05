#include "hal_fakes.h"

DEFINE_FFF_GLOBALS;

HAL_FAKE_LIST(DEFINE_FAKE_VALUE_FUNC, DEFINE_FAKE_VOID_FUNC)

#define RESET_VALUE_FAKE(rtype, name, ...) RESET_FAKE(name);
#define RESET_VOID_FAKE(name, ...)         RESET_FAKE(name);

void hal_fakes_reset(void) {
    HAL_FAKE_LIST(RESET_VALUE_FAKE, RESET_VOID_FAKE)
    FFF_RESET_HISTORY();
}

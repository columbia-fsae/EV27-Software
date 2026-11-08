#include "hal_fakes.h"

#include <string.h>

DEFINE_FFF_GLOBALS;

GPIO_TypeDef hal_fake_gpio_ports[7];

HAL_FAKE_LIST(DEFINE_FAKE_VALUE_FUNC, DEFINE_FAKE_VOID_FUNC)

#define RESET_VALUE_FAKE(rtype, name, ...) RESET_FAKE(name);
#define RESET_VOID_FAKE(name, ...)         RESET_FAKE(name);

void hal_fakes_reset(void) {
    HAL_FAKE_LIST(RESET_VALUE_FAKE, RESET_VOID_FAKE)
    FFF_RESET_HISTORY();
    memset(hal_fake_gpio_ports, 0, sizeof(hal_fake_gpio_ports));
}

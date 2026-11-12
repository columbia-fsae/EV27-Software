#include "fake_soc_flash_storage.h"

FAKE_SOC_FLASH_STORAGE_LIST(DEFINE_FAKE_VALUE_FUNC, DEFINE_FAKE_VOID_FUNC)

#define RESET_VALUE_FAKE(rtype, name, ...) RESET_FAKE(name);
#define RESET_VOID_FAKE(name, ...)         RESET_FAKE(name);

void fake_soc_flash_storage_reset(void) {
    FAKE_SOC_FLASH_STORAGE_LIST(RESET_VALUE_FAKE, RESET_VOID_FAKE)
}

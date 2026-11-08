#include <string.h>

#include "bsm.h"
#include "unity.h"

static bsm_obj bsm;

void setUp(void) {
    memset(&bsm, 0xAA, sizeof(bsm));
    bsm_init(&bsm);
}

void test_init_starts_in_start_dis_with_relays_open(void) {
    TEST_ASSERT_EQUAL_INT8(start_dis, bsm.state);
    TEST_ASSERT_FALSE(bsm.pc_enable);
    TEST_ASSERT_FALSE(bsm.ir_minus_enable);
    TEST_ASSERT_FALSE(bsm.ir_plus_enable);
}

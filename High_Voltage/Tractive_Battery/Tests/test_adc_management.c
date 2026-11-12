/**
 * @file test_adc_management.c
 * @brief Tests for Communications/src/adc_management.c.
 *
 * The DMA hardware is simulated by capturing the buffer the firmware hands to
 * HAL_ADC_Start_DMA, writing raw counts into it, then calling
 * HAL_ADC_ConvCpltCallback the way the DMA interrupt would.
 *
 * ADC2 is started with HAL_DMA_Start, which takes the buffer address as a
 * uint32_t. On a 64-bit PC that cast cuts the pointer in half, so the test
 * cannot write ADC2's buffer and temp_ambient always comes from a raw value
 * of 0.
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "adc_management.h"
#include "comms_config.h"
#include "hal_fakes.h"
#include "isr_sim.h"
#include "unity.h"

static ADC_TypeDef adc1_regs;
static ADC_TypeDef adc2_regs;
static DMA_HandleTypeDef adc2_dma;
static ADC_HandleTypeDef hadc1 = {.Instance = &adc1_regs};
static ADC_HandleTypeDef hadc2 = {.Instance = &adc2_regs, .DMA_Handle = &adc2_dma};

// The ADC1 buffer the firmware gave to DMA in adc_init()
static volatile uint16_t* adc1_dma_buffer;

// Runs adc_init() and remembers the ADC1 DMA buffer, so dma_transfer_complete()
// can write into it. Call it first in any test that uses the callback.
static void start_adcs(void) {
    adc2_regs.ISR = ADC_ISR_ADRDY;  // ADC2 reports ready at once, so the enable wait ends
    adc_init(&hadc1, &hadc2);
    adc1_dma_buffer = (volatile uint16_t*)HAL_ADC_Start_DMA_fake.arg1_val;
}

// What the DMA + interrupt do on the target: fill the buffer, then call back.
static void dma_transfer_complete(const uint16_t raw[ADC1_CHANNEL_COUNT]) {
    for (int i = 0; i < ADC1_CHANNEL_COUNT; i++) {
        adc1_dma_buffer[i] = raw[i];
    }
    HAL_ADC_ConvCpltCallback(&hadc1);
}

void setUp(void) {
    memset(&adc_data, 0, sizeof(adc_data));
    memset(&adc1_regs, 0, sizeof(adc1_regs));
    memset(&adc2_regs, 0, sizeof(adc2_regs));
    adc1_dma_buffer = NULL;
    hal_fakes_reset();
}

void tearDown(void) {}

// ---------------------------------------------------------------- adc_init

void test_init_calibrates_both_adcs_and_starts_dma(void) {
    start_adcs();

    TEST_ASSERT_EQUAL_UINT32(2, HAL_ADCEx_Calibration_Start_fake.call_count);
    TEST_ASSERT_EQUAL_PTR(&hadc1, HAL_ADCEx_Calibration_Start_fake.arg0_history[0]);
    TEST_ASSERT_EQUAL_PTR(&hadc2, HAL_ADCEx_Calibration_Start_fake.arg0_history[1]);

    TEST_ASSERT_EQUAL_UINT32(1, HAL_ADC_Start_DMA_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(ADC1_CHANNEL_COUNT, HAL_ADC_Start_DMA_fake.arg2_val);
    TEST_ASSERT_EQUAL_UINT32(ADC2_CHANNEL_COUNT, HAL_DMA_Start_fake.arg3_val);

    TEST_ASSERT_BITS_HIGH(ADC_CR_ADEN | ADC_CR_ADSTART, adc2_regs.CR);
    TEST_ASSERT_BITS_HIGH(ADC_CFGR_DMAEN | ADC_CFGR_DMACFG, adc2_regs.CFGR);
    TEST_ASSERT_EQUAL_UINT32(0, Error_Handler_fake.call_count);
}

void test_init_reports_error_when_calibration_fails(void) {
    HAL_ADCEx_Calibration_Start_fake.return_val = HAL_ERROR;

    start_adcs();

    TEST_ASSERT_EQUAL_UINT32(2, Error_Handler_fake.call_count);
}

// Error_Handler never returns on the target. Here it does, so this sets ADRDY
// to let adc_init's wait loop exit after the error is reported.
static void set_adc2_ready(void) { adc2_regs.ISR = ADC_ISR_ADRDY; }

void test_init_reports_error_when_adc2_never_ready(void) {
    uint32_t ticks[] = {0, 0, ADC2_ENABLE_TIMEOUT_MS + 1};
    SET_RETURN_SEQ(HAL_GetTick, ticks, 3);
    adc2_regs.ISR = 0;
    Error_Handler_fake.custom_fake = set_adc2_ready;

    adc_init(&hadc1, &hadc2);

    TEST_ASSERT_EQUAL_UINT32(1, Error_Handler_fake.call_count);
}

// ---------------------------------------------------------------- temp_convert

void test_temp_convert_mid_scale_is_25C(void) {
    // Equal pull-up and NTC resistance -> half of Vref -> R/R25 = 1.0
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.0f, temp_convert(2048));
}

void test_temp_convert_interpolates_between_table_points(void) {
    // Halfway between 15C (1.505) and 20C (1.223) is R/R25 = 1.364.
    // R/R25 = raw / (4095 - raw): 2363 / 1732 = 1.3643, 49.9% of the way -> 17.49C.
    // Returning either table point (15 or 20) instead of interpolating fails.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 17.49f, temp_convert(2363));
}

void test_temp_convert_full_scale_clamps_to_minus_10C(void) {
    // 4095 counts: no voltage left across the pull-up (open thermistor)
    TEST_ASSERT_EQUAL_FLOAT(-10.0f, temp_convert(4095));
}

void test_temp_convert_resistance_above_table_clamps_to_minus_10C(void) {
    // 3400 / (4095 - 3400) = 4.892, above the -10C entry (4.651)
    TEST_ASSERT_EQUAL_FLOAT(-10.0f, temp_convert(3400));
}

void test_temp_convert_zero_counts_clamps_to_120C(void) {
    // 0 counts: R/R25 = 0, below the 120C entry (0.05136) (shorted thermistor)
    TEST_ASSERT_EQUAL_FLOAT(120.0f, temp_convert(0));
}

// ---------------------------------------------------------------- callback

void test_callback_converts_every_channel(void) {
    start_adcs();
    const uint16_t raw[ADC1_CHANNEL_COUNT] = {1000, 2000, 2048, 2457, 4095};

    dma_transfer_complete(raw);

    // Voltages: raw * 0.24216 V per count
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 242.16f, adc_data.ts_vsense);   // 1000 * 0.24216
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 484.32f, adc_data.bat_vsense);  // 2000 * 0.24216

    // Temperatures: R/R25 = raw / (4095 - raw), then interpolate the NTC table.
    // 2048 -> R/R25 = 1.0005, 0.22% of the way from 25C (1.0) to 20C (1.223)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.99f, adc_data.temp_precharge);
    // 2457 -> R/R25 = 1.5, 1.8% of the way from 15C (1.505) to 20C (1.223)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 15.09f, adc_data.temp_power);
    // 4095 -> full scale, no voltage across the pull-up: clamps to -10C
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.0f, adc_data.temp_vsense);
}

// ---------------------------------------------------------------- ISR vs main loop
//
// The DMA interrupt writes adc_data one field at a time while the main loop
// (adc_can, bsm_run) reads it one field at a time. If the interrupt lands in
// the middle of a read, the main loop gets some fields from one sample and
// some from the next. This test runs the interrupt nonstop on another thread,
// alternating between two very different samples, and checks that every
// read the main loop makes matches one sample or the other.
//
// When the main loop moves to RTOS tasks, update main_loop_read_adc() (and
// adc_isr() if the ISR changes) to match the new code, then rerun.

#define RACE_READS 1000000

static const uint16_t SAMPLE_A[ADC1_CHANNEL_COUNT] = {1000, 1100, 1200, 1300, 1400};
static const uint16_t SAMPLE_B[ADC1_CHANNEL_COUNT] = {3000, 2900, 2800, 2700, 2600};

static void adc_isr(void) {
    static int toggle;
    toggle ^= 1;
    dma_transfer_complete(toggle ? SAMPLE_A : SAMPLE_B);
}

// How the main loop gets ADC data today: straight from the global, field by
// field. volatile stops the compiler caching the values across loop passes,
// which the real main loop can't do either since it reads from another file.
static ADC_Inputs_t main_loop_read_adc(void) {
    const volatile ADC_Inputs_t* src = &adc_data;
    ADC_Inputs_t out;
    out.ts_vsense = src->ts_vsense;
    out.bat_vsense = src->bat_vsense;
    out.temp_ambient = src->temp_ambient;
    out.temp_vsense = src->temp_vsense;
    out.temp_power = src->temp_power;
    out.temp_precharge = src->temp_precharge;
    return out;
}

static bool same_sample(const ADC_Inputs_t* a, const ADC_Inputs_t* b) {
    return a->ts_vsense == b->ts_vsense && a->bat_vsense == b->bat_vsense &&
           a->temp_ambient == b->temp_ambient && a->temp_vsense == b->temp_vsense &&
           a->temp_power == b->temp_power && a->temp_precharge == b->temp_precharge;
}

void test_main_loop_never_reads_half_updated_adc_data(void) {
    start_adcs();
    // Work out what each whole sample looks like after conversion
    dma_transfer_complete(SAMPLE_B);
    const ADC_Inputs_t expect_b = adc_data;
    dma_transfer_complete(SAMPLE_A);
    const ADC_Inputs_t expect_a = adc_data;

    isr_sim_install_irq_fakes();
    isr_sim_start(adc_isr);

    uint32_t torn = 0, saw_a = 0, saw_b = 0;
    ADC_Inputs_t first_torn = {0};
    for (uint32_t i = 0; i < RACE_READS; i++) {
        ADC_Inputs_t got = main_loop_read_adc();
        if (same_sample(&got, &expect_a)) {
            saw_a++;
        } else if (same_sample(&got, &expect_b)) {
            saw_b++;
        } else if (torn++ == 0) {
            first_torn = got;
        }
    }

    uint32_t isr_runs = isr_sim_stop();

    // Make sure the two threads actually overlapped, otherwise a pass means nothing
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(1000, isr_runs, "ISR thread barely ran");
    TEST_ASSERT_TRUE_MESSAGE(saw_a > 0 && saw_b > 0, "main loop never saw both samples");

    if (torn > 0) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "%lu of %d reads mixed two samples (ISR ran %lu times). First one: "
                 "ts=%.1f bat=%.1f precharge=%.1f power=%.1f vsense=%.1f",
                 (unsigned long)torn, RACE_READS, (unsigned long)isr_runs, first_torn.ts_vsense,
                 first_torn.bat_vsense, first_torn.temp_precharge, first_torn.temp_power,
                 first_torn.temp_vsense);
        TEST_FAIL_MESSAGE(msg);
    }
}

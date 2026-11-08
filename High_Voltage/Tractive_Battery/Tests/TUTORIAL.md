# Unit testing the Tractive Battery firmware

A tutorial for team members who know C but have never written a unit test,
faked hardware, or used Unity or FFF. By the end you can run the tests, add a
test for a new module, add the fakes it needs, and read the results.

Every code sample here was compiled and run with the harness in this folder.
The output shown is real.

## Contents

1. [Run the tests you already have](#1-run-the-tests-you-already-have)
2. [The ideas: unit tests, fakes, Unity, FFF](#2-the-ideas-unit-tests-fakes-unity-fff)
3. [How the harness fits together](#3-how-the-harness-fits-together)
4. [Checklist: adding tests](#4-checklist-adding-tests)
5. [Your first firmware test: the battery state machine](#5-your-first-firmware-test-the-battery-state-machine)
6. [Faking another firmware module](#6-faking-another-firmware-module)
7. [FFF feature tour (a practice module)](#7-fff-feature-tour-a-practice-module)
8. [How do I know I need a new fake?](#8-how-do-i-know-i-need-a-new-fake)
9. [When a test finds a bug](#9-when-a-test-finds-a-bug)
10. [Static functions and static state](#10-static-functions-and-static-state)
11. [Testing data shared with an interrupt](#11-testing-data-shared-with-an-interrupt)
12. [Debugging a failing test](#12-debugging-a-failing-test)
13. [Common mistakes](#13-common-mistakes)
14. [Sanitizers, -Werror, coverage and CI](#14-sanitizers--werror-coverage-and-ci)
15. [References](#15-references)

---

## 1. Run the tests you already have

Run every command in this tutorial from `High_Voltage/Tractive_Battery`.

```sh
cmake -S Tests -B Tests/build          # configure (once, or after editing CMakeLists.txt)
cmake --build Tests/build              # build every test
ctest --test-dir Tests/build --output-on-failure   # run them
```

You need CMake 3.22+ and a host C compiler (gcc or clang), not
`arm-none-eabi-gcc`. On Linux or WSL: `sudo apt install build-essential cmake
ninja-build`.

<details>
<summary>Windows without a C compiler (zig, via pip)</summary>

```sh
py -3 -m venv .venv-tests
.venv-tests/Scripts/python -m pip install ziglang cmake ninja
```

CMake also needs an archiver, which zig only has as a subcommand. Make two
one-line scripts:

```bat
:: zig-ar.cmd
@"<path to venv>\Lib\site-packages\ziglang\zig.exe" ar %*
:: zig-ranlib.cmd
@"<path to venv>\Lib\site-packages\ziglang\zig.exe" ranlib %*
```

Then configure with:

```sh
CC="<path to zig.exe> cc" cmake -S Tests -B Tests/build -G Ninja \
    -DCMAKE_AR=<path>/zig-ar.cmd -DCMAKE_RANLIB=<path>/zig-ranlib.cmd
```

Sanitizers and coverage don't work with zig on Windows. CI runs them for you
(section 14).
</details>

Each test module is one executable, so ctest prints one line per module. To
see every test function's result, run the executable itself:

```sh
Tests/build/test_adc_management        # .exe on Windows
```

```
test_adc_management.c:63:test_init_calibrates_both_adcs_and_starts_dma:PASS
...
test_adc_management.c:214:test_main_loop_never_reads_half_updated_adc_data:FAIL:378652 of 1000000 reads mixed two samples ...
```

Each line is `file:line:test:result`. That last failure is real and expected.
It found a race in the firmware (section 11). Don't "fix" it by changing the
test.

Useful variations:

| Command | What it does |
|---|---|
| `ctest --test-dir Tests/build -R bsm --output-on-failure` | Only test executables whose name matches `bsm` |
| `ctest --test-dir Tests/build --rerun-failed --output-on-failure` | Only what failed last time |
| `ctest --test-dir Tests/build --repeat until-fail:50 -R adc` | Hunt a flaky test |

## 2. The ideas: unit tests, fakes, Unity, FFF

A **unit test** calls one function or module with inputs you choose and checks
the result. It runs on your PC in milliseconds, with no board attached.

Firmware is hard to test this way because it talks to hardware. `bsm_run()`
calls `HAL_GetTick()`, and a PC has no SysTick timer. So in a test, the real
HAL is replaced by **fakes**: functions with the same name and signature that
the test controls. The firmware's code doesn't change. It is compiled against
the fakes instead of the real HAL. (This trick is called a *link seam*.)

Two libraries do the work. Both are in `vendor/` and should never be edited.

- **[Unity](https://github.com/ThrowTheSwitch/Unity)** runs tests and checks
  results: `TEST_ASSERT_EQUAL_UINT32(expected, actual)` and about 100 more
  assertions.
- **[FFF](https://github.com/meekrosoft/fff)** (Fake Function Framework)
  creates fakes from one line of macro. This line:

  ```c
  FAKE_VOID_FUNC(heater_set, bool);
  ```

  expands to roughly this:

  ```c
  typedef struct {
      bool arg0_val;                          // argument of the most recent call
      bool arg0_history[FFF_ARG_HISTORY_LEN]; // arguments of the first 50 calls
      unsigned int call_count;
      void (*custom_fake)(bool);              // if set, runs on every call
      /* ...sequences, history counters... */
  } heater_set_Fake;

  heater_set_Fake heater_set_fake;            // what your test looks at

  void heater_set(bool arg0) {                // what the firmware calls
      heater_set_fake.arg0_val = arg0;
      /* record history, ++call_count, run custom_fake if set */
  }
  ```

  A fake that returns a value, `FAKE_VALUE_FUNC(int, sensor_read, uint8_t,
  float*)`, also gets a `return_val` you can set.

So a fake is an ordinary C function with a global struct beside it. The test
sets the struct before the call (what to return) and reads it after (was I
called, with what).

## 3. How the harness fits together

```
Tests/
├── CMakeLists.txt          add_fw_test(<module> ...) lines: one per tested module
├── test_<module>.c         Your tests. No main(): the build generates it
├── TUTORIAL.md             This file
├── cmake/
│   ├── runner.cmake        Writes each test's main() (the "runner")
│   └── quality.cmake       Sanitizer, -Werror and coverage options
├── tools/needs_fakes.py    Tells you what a module needs to link
├── fakes/
│   ├── stm32g4xx_hal.h     HAL types, register bits and constants (no real HAL)
│   ├── main.h              Replaces Core/Inc/main.h
│   ├── hal_fakes.{h,c}     HAL_FAKE_LIST: every faked HAL/CMSIS function
│   └── fake_<module>.{h,c} Fakes for whole firmware modules (FAKES <module>)
├── support/isr_sim.{h,c}   Simulated interrupt for race tests
└── vendor/                 Unity and FFF. Don't edit.
```

What happens when you build `test_bsm`:

1. CMake reads `test_bsm.c` and finds every `void test_<name>(void)` that
   starts at column 0.
2. It writes a runner, `build/runners/test_bsm_runner.c`, with a `main()`
   that does this for each test:

   ```c
   hal_fakes_reset();         // every HAL fake back to zero
   fake_<x>_reset();          // one per FAKES entry
   RUN_TEST(test_<name>);     // Unity calls setUp(), the test, then tearDown()
   ```

3. It compiles `test_bsm.c`, the runner, the real `BSM/src/bsm.c` (found
   automatically), any `SOURCES`/`FAKES` you listed, and links Unity and the
   HAL fakes.

Because fakes are reset *before* `setUp()`, everything you set in `setUp()`
stays set for the test. You never need to reset fakes yourself, except local
fakes you declare in a test file (section 7).

Because `fakes/` comes first on the include path, `#include "main.h"` and
`#include "stm32g4xx_hal.h"` in firmware code pick up the stand-ins. That's why
firmware compiles unchanged.

## 4. Checklist: adding tests

### A test file for a new module

1. **Ask what the module needs.** Configure once (section 1), then:

   ```sh
   python Tests/tools/needs_fakes.py <module>
   ```

   `<module>` is the `.c` file's name without `.c`, e.g. `bsm`. The tool lists
   every function the module calls from outside, says where each one comes
   from, and prints the CMake line to use. If anything is `MISSING`, add it
   first (section 8).
2. **Create `Tests/test_<module>.c`.** Start from this skeleton:

   ```c
   #include "<module>.h"
   #include "unity.h"

   void setUp(void) {
       // put the module in a known state; fakes are already reset
   }

   void test_<what>_<expected_behaviour>(void) {
       // arrange, act, assert
   }
   ```

   Add `#include "hal_fakes.h"` or `#include "fake_<x>.h"` when a test touches
   those fakes. `setUp()` and `tearDown()` are optional.
3. **Register it.** Paste the line `needs_fakes.py` printed into
   `Tests/CMakeLists.txt`, under the other `add_fw_test` lines.
4. **Configure, build and run.**

   ```sh
   cmake -S Tests -B Tests/build
   cmake --build Tests/build
   ctest --test-dir Tests/build -R <module> --output-on-failure
   ```

5. **Watch it fail once.** Break an assertion on purpose and check it fails.
   A test that can't fail proves nothing.
6. Commit the test file and the CMake line together.

### A new test in an existing file

1. Add a `void test_<name>(void)` function at column 0.
2. `cmake --build Tests/build`. The runner is regenerated automatically.
3. Run it.

### When the module needs something new

| Need | What to add | Section |
|---|---|---|
| A HAL type, register bit or macro | It, in `fakes/stm32g4xx_hal.h` | 8 |
| A HAL or CMSIS function | One line in `HAL_FAKE_LIST` (`fakes/hal_fakes.h`) | 8 |
| Another firmware module, faked | `FAKES <module>` (write `fakes/fake_<module>.{h,c}` if it doesn't exist) | 6, 8 |
| Another firmware module, real | `SOURCES <module>`, then rerun `needs_fakes.py <module> --with <other>` | 6 |
| A preprocessor define | `DEFINES <macro>` | 8 |

## 5. Your first firmware test: the battery state machine

[bsm.c](../BSM/src/bsm.c) calls no hardware except `HAL_GetTick()`, so it's
the easiest firmware module to start with. It controls the contactors, so it
also matters most.

### Step 1: what does it need?

```sh
python Tests/tools/needs_fakes.py bsm
```

```
What bsm needs from other code:

  symbol       provided by  how to link it
  -----------  -----------  --------------
  HAL_GetTick  HAL fake     fakes/hal_fakes.h

Suggested CMake line (Tests/CMakeLists.txt):

    add_fw_test(bsm)
```

Only `HAL_GetTick`, and the HAL fakes are always linked. So there's nothing to
add.

### Step 2: one test

Create `Tests/test_bsm.c`:

```c
#include <string.h>

#include "bsm.h"
#include "unity.h"

static bsm_obj bsm;

void setUp(void) {
    memset(&bsm, 0xAA, sizeof(bsm));  // garbage, so the test proves bsm_init sets it
    bsm_init(&bsm);
}

void test_init_starts_in_start_dis_with_relays_open(void) {
    TEST_ASSERT_EQUAL_INT8(start_dis, bsm.state);
    TEST_ASSERT_FALSE(bsm.pc_enable);
    TEST_ASSERT_FALSE(bsm.ir_minus_enable);
    TEST_ASSERT_FALSE(bsm.ir_plus_enable);
}
```

Three habits:

- **Arrange, act, assert.** Set up inputs, call one thing, then check.
- **Name the test after the behaviour you expect.** The failure line prints
  the name, so it already says what broke.
- **One behaviour per test.** Several asserts are fine if they check one idea.

### Step 3: register, build, run

Add the suggested line to `Tests/CMakeLists.txt`:

```cmake
add_fw_test(bsm)
```

```sh
cmake -S Tests -B Tests/build
cmake --build Tests/build
ctest --test-dir Tests/build -R bsm --output-on-failure
```

### Step 4: control time

Most of `bsm_run()` depends on how much time has passed. Give `HAL_GetTick`
a `custom_fake` (a function FFF runs instead of returning `return_val`) that
returns a variable the test sets. Here is the file grown to cover the main
transitions:

```c
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsm.h"
#include "hal_fakes.h"
#include "unity.h"

static bsm_obj bsm;
static GPIO_Info_t gpio;
static ADC_Inputs_t adc;

// HAL_GetTick() returns whatever the test sets here.
static uint32_t now_ms;
static uint32_t fake_tick(void) { return now_ms; }

void setUp(void) {
    HAL_GetTick_fake.custom_fake = fake_tick;  // fakes were reset just before setUp
    now_ms = 0;
    memset(&gpio, 0, sizeof(gpio));
    memset(&adc, 0, sizeof(adc));
    bsm_init(&bsm);
}

// ------------------------------------------------------------- helpers

static void shutdown_circuit_ok(void) {
    gpio.sdc_ok = true;
    gpio.bms_ok_OUT = true;
}

static void run_at(uint32_t t_ms) {
    now_ms = t_ms;
    bsm_run(&bsm, &gpio, &adc);
}

static void assert_relays(bool pc, bool ir_minus, bool ir_plus) {
    TEST_ASSERT_EQUAL_MESSAGE(pc, bsm.pc_enable, "precharge relay");
    TEST_ASSERT_EQUAL_MESSAGE(ir_minus, bsm.ir_minus_enable, "IR- relay");
    TEST_ASSERT_EQUAL_MESSAGE(ir_plus, bsm.ir_plus_enable, "IR+ relay");
}

// Drive the machine from start_dis into precharge.
static void go_to_precharge(void) {
    shutdown_circuit_ok();
    run_at(0);  // start_dis -> ir_minus_close
    gpio.ir_minus_aux = true;
    run_at(10);  // ir_minus_close -> precharge
    TEST_ASSERT_EQUAL_INT8(precharge, bsm.state);
}

// ----------------------------------------------------------------- tests

void test_init_starts_in_start_dis_with_relays_open(void) {
    TEST_ASSERT_EQUAL_INT8(start_dis, bsm.state);
    assert_relays(false, false, false);
}

void test_stays_in_start_dis_while_shutdown_circuit_open(void) {
    run_at(0);

    TEST_ASSERT_EQUAL_INT8(start_dis, bsm.state);
}

void test_closes_ir_minus_when_shutdown_circuit_ok(void) {
    shutdown_circuit_ok();

    run_at(0);  // the transition happens on this call...
    TEST_ASSERT_EQUAL_INT8(ir_minus_close, bsm.state);
    run_at(1);  // ...and the outputs change on the next one
    assert_relays(false, true, false);
}

void test_faults_when_ir_minus_aux_never_confirms(void) {
    shutdown_circuit_ok();
    run_at(0);

    run_at(IR_FAULT_TIME_MS);  // not yet: the check is strictly greater
    TEST_ASSERT_EQUAL_INT8(ir_minus_close, bsm.state);

    run_at(IR_FAULT_TIME_MS + 1);
    TEST_ASSERT_EQUAL_INT8(FAULT, bsm.state);
}

void test_precharge_waits_min_time_even_when_voltage_matches(void) {
    go_to_precharge();
    adc.bat_vsense = 400.0f;
    adc.ts_vsense = 400.0f;

    run_at(10 + PRECHARGE_MIN_TIME_MS);

    TEST_ASSERT_EQUAL_INT8(precharge, bsm.state);
}

void test_precharge_completes_when_voltage_in_window_after_min_time(void) {
    go_to_precharge();
    adc.bat_vsense = 400.0f;
    adc.ts_vsense = 380.0f;  // 95% of battery voltage

    run_at(10 + PRECHARGE_MIN_TIME_MS + 1);

    TEST_ASSERT_EQUAL_INT8(ir_plus_close, bsm.state);
}

void test_every_state_returns_to_start_dis_when_shutdown_opens(void) {
    const uint8_t states[] = {ir_minus_close, precharge, ir_plus_close, delay_post_pc, driving};

    for (size_t i = 0; i < sizeof(states); i++) {
        bsm.state = states[i];
        bsm.timer = 0;
        gpio.sdc_ok = false;
        gpio.ir_minus_aux = true;
        gpio.ir_plus_aux = true;

        run_at(5);

        char msg[32];
        snprintf(msg, sizeof(msg), "from state %u", states[i]);
        TEST_ASSERT_EQUAL_INT8_MESSAGE(start_dis, bsm.state, msg);
    }
}

void test_fault_is_latched(void) {
    bsm.state = FAULT;
    shutdown_circuit_ok();
    gpio.ir_minus_aux = true;
    gpio.ir_plus_aux = true;

    run_at(1000000);

    TEST_ASSERT_EQUAL_INT8(FAULT, bsm.state);
    assert_relays(false, false, false);
}

void test_ir_minus_timeout_survives_tick_wraparound(void) {
    shutdown_circuit_ok();
    run_at(UINT32_MAX - 100);  // enter ir_minus_close just before the tick wraps

    run_at(UINT32_MAX - 50);  // only 50 ms later

    TEST_ASSERT_EQUAL_INT8(ir_minus_close, bsm.state);
}
```

Why `custom_fake` and not `SET_RETURN_SEQ` (a list of return values)?
`bsm_run()` calls `HAL_GetTick()` a different number of times on each
branch. A fixed list would get out of step.

What to notice:

- **Helpers** (`run_at`, `go_to_precharge`, `assert_relays`) leave each test
  showing only what's different about it. Make them `static`, and don't
  start their names with `test_`, or the runner will treat them as tests.
- **Both sides of each edge.** The IR− timeout is checked at exactly
  `IR_FAULT_TIME_MS` and 1 ms later. Off-by-one bugs (`>` vs `>=`) hide there.
- **The `_MESSAGE` assertion** in the loop names the state that failed.
  Without it you'd only get a line number.
- **Config names, not numbers** (`PRECHARGE_MIN_TIME_MS`), so retuning
  `bsm_config.h` doesn't break tests.

Run it and the last test fails. Section 9 explains why that's a good thing.
[TO_TEST.md](TO_TEST.md) lists more BSM cases worth writing.

## 6. Faking another firmware module

Firmware modules call each other. When you test `kalman_soc.c`, it calls
`battery_model.c`. You have two choices:

- **Link the real module** (`SOURCES battery_model`). Best when it is simple
  and deterministic, like a lookup table. The test then checks both together.
- **Fake it** (`FAKES battery_model`). Best when you want to force an answer
  the real module can't easily give, or when the real module talks to
  hardware.

Every fake library lives in `fakes/fake_<module>.h` and `.c`. Never copy its
fakes into your test file. Ask the tool what is available:

```sh
python Tests/tools/needs_fakes.py kalman_soc
```

```
What kalman_soc needs from other code:

  symbol                      provided by         how to link it
  --------------------------  ------------------  --------------
  BatteryModel_GetOcv         fake_battery_model  FAKES battery_model (real: ADBMS6830/program/src/battery_model.c)
  BatteryModel_GetSlope       fake_battery_model  FAKES battery_model (real: ADBMS6830/program/src/battery_model.c)
  BatteryModel_GetSocFromOcv  fake_battery_model  FAKES battery_model (real: ADBMS6830/program/src/battery_model.c)

Suggested CMake line (Tests/CMakeLists.txt):

    add_fw_test(kalman_soc FAKES battery_model)
```

A test that fakes the OCV lookup:

```c
#include "fake_battery_model.h"
#include "kalman_soc.h"
#include "unity.h"

static KalmanSOC kf;

void test_init_from_voltage_looks_up_soc_from_rest_voltage(void) {
    BatteryModel_GetSocFromOcv_fake.return_val = 0.8f;

    KalmanSOC_InitFromVoltage(&kf, 3.95f, 25.0f);

    TEST_ASSERT_EQUAL_UINT32(1, BatteryModel_GetSocFromOcv_fake.call_count);
    TEST_ASSERT_EQUAL_FLOAT(3.95f, BatteryModel_GetSocFromOcv_fake.arg0_val);
    TEST_ASSERT_EQUAL_FLOAT(0.8f, kf.x.soc);
    TEST_ASSERT_TRUE(kf.initialized);
}

void test_init_from_voltage_uses_default_soc_when_lookup_is_negative(void) {
    BatteryModel_GetSocFromOcv_fake.return_val = -1.0f;

    KalmanSOC_InitFromVoltage(&kf, 0.0f, 25.0f);

    TEST_ASSERT_EQUAL_FLOAT(KALMAN_DEFAULT_INITIAL_SOC, kf.x.soc);
}

// A custom_fake can stand in for a whole curve: here a straight line from
// 3.0 V (empty) to 4.2 V (full).
static float linear_soc(float ocv) { return (ocv - 3.0f) / 1.2f; }

void test_init_from_voltage_with_a_linear_ocv_curve(void) {
    BatteryModel_GetSocFromOcv_fake.custom_fake = linear_soc;

    KalmanSOC_InitFromVoltage(&kf, 3.6f, 25.0f);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, kf.x.soc);
}
```

```cmake
add_fw_test(kalman_soc FAKES battery_model)
```

The second test reaches a branch (`initial_soc < 0`) the real lookup table
would never take. That's what fakes are for.

Fake libraries that exist today:

| `FAKES` entry | Fakes | Also defines (zeroed by the reset) |
|---|---|---|
| `can_management` | `can_init`, `CAN_SendData`, the `bms_/soc_/adc_can` senders | `can_data`, `error_info` |
| `adBms_Application` | `measurement_loop`, `adbms_main_init`, `adBms_main_run`, `adBms6830_soc_*`, `lookupID`, `cell_to_temp_index` | `PackSegments`, `TotalPack`, `g_soc_estimate`, `IC`, ... |
| `adbms_driver` | The ADBMS6830 chip: `adBmsReadData/WriteData`, ADC start commands, `adBmsWakeupIc`, `Delay_ms` | none |
| `kalman_soc` | Every `KalmanSOC_*` function | none |
| `soc_flash_storage` | `SOC_Flash_Save/Load/Erase/HasValidData` | none |
| `battery_model` | Every `BatteryModel_*` function | none |

Each header's comment says which functions need a `custom_fake` to be
useful. These are mostly the ones that return data through a pointer, such
as `adBmsReadData` filling `ic[]` with cell voltages.

**Using the real module instead.** Linking a real module also links what *it*
needs. `--with` shows that:

```sh
python Tests/tools/needs_fakes.py soc_flash_storage --with kalman_soc
```

lists what `soc_flash_storage` and `kalman_soc` need together, and suggests
`add_fw_test(soc_flash_storage SOURCES kalman_soc FAKES battery_model)`.

You can't list one module in both `SOURCES` and `FAKES`: CMake stops with an
error. Linking both would define every function twice.

## 7. FFF feature tour (a practice module)

This section uses a tiny made-up module, so each FFF feature can be shown on
its own. Its dependencies don't exist anywhere, so the test file declares
**local fakes** with `FAKE_VALUE_FUNC` / `FAKE_VOID_FUNC`. That's the right
tool for a one-off like this. For real firmware modules, use the fake
libraries from section 6.

Put these three files in `Tests/` while you practise. Don't commit them.

```c
// thermostat.h
#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <stdbool.h>
#include <stdint.h>

// Dependencies: implemented somewhere else (a driver, the HAL...)
int sensor_read(uint8_t channel, float* out_celsius);  // 0 = OK
void heater_set(bool on);
void log_msg(const char* fmt, ...);

// The code under test
void thermostat_step(float setpoint_c);

#endif
```

```c
// thermostat.c
#include "thermostat.h"

void thermostat_step(float setpoint_c) {
    float t;
    if (sensor_read(0, &t) != 0) {
        heater_set(false);  // fail safe: heater off if we can't read
        log_msg("sensor %d failed", 0);
        return;
    }
    heater_set(t < setpoint_c);
}
```

Register it with `NO_DEFAULT_SOURCE`, because `thermostat.c` isn't in a
firmware folder. `SOURCES` paths are relative to `Tractive_Battery/`:

```cmake
add_fw_test(thermostat NO_DEFAULT_SOURCE SOURCES Tests/thermostat.c)
```

### 7.1 Call counts and arguments

```c
// test_thermostat.c
#include <string.h>

#include "fff.h"
#include "thermostat.h"
#include "unity.h"

// Local fakes: only for this practice module, whose dependencies are made up.
// Firmware modules get theirs from fakes/fake_<module>.h instead.
FAKE_VALUE_FUNC(int, sensor_read, uint8_t, float*);
FAKE_VOID_FUNC(heater_set, bool);
FAKE_VOID_FUNC_VARARG(log_msg, const char*, ...);

// The runner resets the HAL fakes and fake libraries before each test, but it
// doesn't know about local fakes, so reset them here.
void setUp(void) {
    RESET_FAKE(sensor_read);
    RESET_FAKE(heater_set);
    RESET_FAKE(log_msg);
}

void test_reads_channel_0_once(void) {
    thermostat_step(20.0f);  // act

    TEST_ASSERT_EQUAL_UINT32(1, sensor_read_fake.call_count);  // assert
    TEST_ASSERT_EQUAL_UINT8(0, sensor_read_fake.arg0_val);
}
```

```
test_thermostat.c:22:test_reads_channel_0_once:PASS
```

Arguments are numbered from 0: `arg0_val` is the first argument of the most
recent call.

### 7.2 Return values

A fake's `return_val` starts at zero. Set it to steer the code down another
path:

```c
void test_heater_off_and_logged_when_sensor_fails(void) {
    sensor_read_fake.return_val = -1;  // arrange: the sensor fails

    thermostat_step(20.0f);

    TEST_ASSERT_EQUAL_UINT32(1, heater_set_fake.call_count);
    TEST_ASSERT_FALSE(heater_set_fake.arg0_val);  // fail safe
    TEST_ASSERT_EQUAL_UINT32(1, log_msg_fake.call_count);
    TEST_ASSERT_EQUAL_STRING("sensor %d failed", log_msg_fake.arg0_val);
}
```

The firmware tests reach error paths the same way, for example
`HAL_ADCEx_Calibration_Start_fake.return_val = HAL_ERROR` in
`test_adc_management.c`.

### 7.3 Custom fakes and output parameters

`sensor_read` returns its reading through a pointer. `return_val` can't fill
that in, so give the fake a function to run instead. It must have exactly the
same signature as the faked function:

```c
static float fake_temp_c;

static int sensor_read_returns_fake_temp(uint8_t channel, float* out_celsius) {
    (void)channel;
    *out_celsius = fake_temp_c;
    return 0;
}

void test_heater_on_when_below_setpoint(void) {
    fake_temp_c = 15.0f;
    sensor_read_fake.custom_fake = sensor_read_returns_fake_temp;

    thermostat_step(20.0f);

    TEST_ASSERT_TRUE(heater_set_fake.arg0_val);
}

void test_heater_off_at_exactly_setpoint(void) {
    fake_temp_c = 20.0f;  // the boundary: `t < setpoint` is false here
    sensor_read_fake.custom_fake = sensor_read_returns_fake_temp;

    thermostat_step(20.0f);

    TEST_ASSERT_FALSE(heater_set_fake.arg0_val);
}
```

When a `custom_fake` is set, `return_val` is ignored. FFF still records the
arguments and the call count. The second test checks a **boundary**, the
exact value where behaviour changes.

### 7.4 Several calls: sequences and history

```c
static int read_15(uint8_t ch, float* out) { (void)ch; *out = 15.0f; return 0; }
static int read_25(uint8_t ch, float* out) { (void)ch; *out = 25.0f; return 0; }
static int read_fail(uint8_t ch, float* out) { (void)ch; (void)out; return -1; }

void test_follows_temperature_over_three_steps(void) {
    int (*readings[])(uint8_t, float*) = {read_15, read_25, read_fail};
    SET_CUSTOM_FAKE_SEQ(sensor_read, readings, 3);

    thermostat_step(20.0f);
    thermostat_step(20.0f);
    thermostat_step(20.0f);

    TEST_ASSERT_EQUAL_UINT32(3, heater_set_fake.call_count);
    TEST_ASSERT_TRUE(heater_set_fake.arg0_history[0]);   // 15 < 20
    TEST_ASSERT_FALSE(heater_set_fake.arg0_history[1]);  // 25 > 20
    TEST_ASSERT_FALSE(heater_set_fake.arg0_history[2]);  // sensor failed
}

void test_return_sequence_repeats_last_value(void) {
    int results[] = {0, -1};
    SET_RETURN_SEQ(sensor_read, results, 2);

    TEST_ASSERT_EQUAL_INT(0, sensor_read(0, NULL));
    TEST_ASSERT_EQUAL_INT(-1, sensor_read(0, NULL));
    TEST_ASSERT_EQUAL_INT(-1, sensor_read(0, NULL));  // last value repeats
}
```

- When a sequence runs out, the **last entry repeats**.
- History keeps the first 50 calls (`FFF_ARG_HISTORY_LEN`). Later calls are
  only counted, in `arg_histories_dropped`.

### 7.5 Call order across fakes

Sometimes order is the point: open one relay *before* closing another. FFF
records every fake call in `fff.call_history`, and the runner clears it
before each test:

```c
void test_reads_sensor_before_driving_heater(void) {
    thermostat_step(20.0f);

    TEST_ASSERT_EQUAL_PTR((void*)sensor_read, fff.call_history[0]);
    TEST_ASSERT_EQUAL_PTR((void*)heater_set, fff.call_history[1]);
}
```

Only check order when order matters. Otherwise the test breaks every time
someone reorders harmless calls.

### 7.6 Variadic functions (printf-style)

`log_msg` was faked with `FAKE_VOID_FUNC_VARARG` (`FAKE_VALUE_FUNC_VARARG` if
it returns a value). FFF records the fixed arguments (`arg0_val` is the
format string) but not the `...` part. The firmware calls `printf` directly.
Leave that real rather than testing console output.

### 7.7 Pointer arguments: copy the data, not the pointer

For a pointer argument, FFF saves the *pointer*, not what it points to. That
is a trap for code that builds a CAN payload in a local array: once the
function returns, the array is gone. Copy the bytes inside a `custom_fake`,
while the caller's buffer still exists:

```c
// Pointer arguments: a dependency and code under test, inline to keep it short.
int can_send(uint16_t id, const uint8_t* data, uint8_t len);
FAKE_VALUE_FUNC(int, can_send, uint16_t, const uint8_t*, uint8_t);

static void send_voltage(float volts) {
    uint8_t data[2];  // on the stack: gone once this function returns
    uint16_t raw = (uint16_t)(volts * 10.0f);
    data[0] = raw & 0xFF;
    data[1] = raw >> 8;
    can_send(0x100, data, 2);
}

static uint8_t sent[8];
static uint8_t sent_len;

static int can_send_capture(uint16_t id, const uint8_t* data, uint8_t len) {
    (void)id;
    memcpy(sent, data, len);  // copy while the caller's buffer still exists
    sent_len = len;
    return 0;
}

void test_voltage_sent_little_endian_in_tenths(void) {
    RESET_FAKE(can_send);
    can_send_fake.custom_fake = can_send_capture;

    send_voltage(400.0f);

    const uint8_t expected[] = {0xA0, 0x0F};  // 4000 = 0x0FA0
    TEST_ASSERT_EQUAL_UINT16(0x100, can_send_fake.arg0_val);
    TEST_ASSERT_EQUAL_UINT8(2, sent_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, sent, 2);
}
```

`TEST_ASSERT_EQUAL_HEX8_ARRAY` prints the first byte that differs, which makes
byte-order mistakes easy to spot. For the real CAN code, use the same pattern
with `CAN_SendData_fake` from `fake_can_management.h`.

### 7.8 Cheat sheet

| On `<name>_fake` | Use |
|---|---|
| `.return_val` | What the fake returns |
| `.custom_fake` | Run your own function instead (output params, registers, state) |
| `SET_RETURN_SEQ(name, array, n)` / `SET_CUSTOM_FAKE_SEQ` | A different result per call; the last one repeats |
| `.call_count`, `.argN_val` | How often it was called, the last call's arguments |
| `.argN_history[i]` | Argument N of call i (first 50 calls) |
| `fff.call_history[i]` | Which fake was called i-th |

Common Unity assertions: `TEST_ASSERT_EQUAL_UINT32`, `TEST_ASSERT_FLOAT_WITHIN`,
`TEST_ASSERT_EQUAL_PTR`, `TEST_ASSERT_BITS_HIGH`, `TEST_ASSERT_EQUAL_HEX8_ARRAY`,
`TEST_ASSERT_EQUAL_STRING`, and a `_MESSAGE` variant of each. The full list is
in the [assertions reference](https://github.com/ThrowTheSwitch/Unity/blob/v2.7.0/docs/UnityAssertionsReference.md).

When you're done practising, delete `Tests/thermostat.{h,c}`,
`Tests/test_thermostat.c` and the `add_fw_test(thermostat ...)` line.

## 8. How do I know I need a new fake?

**Run the tool first.** `python Tests/tools/needs_fakes.py <module>` answers
the question before you write a test. It exits with:

- **0**: everything links. Use the suggested `add_fw_test` line.
- **1**: a firmware function or variable is `MISSING`. The tool prints the
  line to add and the fake library it goes in.
- **2**: the module didn't compile. The tool prints the compiler errors. The
  stand-in HAL only declares the functions in `HAL_FAKE_LIST`, so a HAL
  function nobody has faked yet shows up here, as "undeclared". For those the
  tool also prints the line to paste. For example, `soc_flash_storage` with
  its target defines:

  ```
  Undeclared functions: add a fake for each, then rerun.

    VALUE(HAL_StatusTypeDef, HAL_FLASH_Unlock)
        -> HAL_FAKE_LIST in Tests/fakes/hal_fakes.h  (declared in Drivers/STM32G4xx_HAL_Driver/Inc/stm32g4xx_hal_flash.h)
    VALUE(HAL_StatusTypeDef, HAL_FLASHEx_Erase, FLASH_EraseInitTypeDef*, uint32_t*)
        -> HAL_FAKE_LIST in Tests/fakes/hal_fakes.h  (declared in Drivers/STM32G4xx_HAL_Driver/Inc/stm32g4xx_hal_flash_ex.h)
  ```

  Type errors (like `FLASH_EraseInitTypeDef` above) need the type copied into
  `fakes/stm32g4xx_hal.h` first. Fix them, paste the lines, and rerun until
  the exit code is 0.

If you skip the tool, the build tells you the same things, just less
politely. Each kind of error comes from a different step and has a different
fix:

| Error looks like | Step | Cause | Fix |
|---|---|---|---|
| `unknown type name 'FDCAN_RxHeaderTypeDef'`, `use of undeclared identifier 'FLASH_TYPEERASE_PAGES'`, `no member named 'Init'` | Compile | A HAL type, register, field or macro is missing from the stand-in HAL | Copy it from `Drivers/` into `fakes/stm32g4xx_hal.h`. Only add the fields the firmware touches |
| `use of undeclared identifier 'Fan_EN_Pin'` | Compile | A CubeMX pin name that isn't in `fakes/main.h` yet (new pin in the `.ioc`) | Copy it from `Core/Inc/main.h` into `fakes/main.h` |
| `call to undeclared function 'HAL_FLASH_Unlock'` | Compile | A HAL function the stand-in doesn't declare | Add one line to `HAL_FAKE_LIST` (it declares and defines the fake) |
| `undefined reference to 'KalmanSOC_Update'` (gcc), `undefined symbol` (clang/lld) | Link | Nothing defines a function the module calls | Add `FAKES <module>` or `SOURCES <module>`, or a line to that module's fake list |
| `multiple definition of 'KalmanSOC_Init'` (gcc), `duplicate symbol` (clang/lld) | Link | Two things define it: the real module and its fake, or a local fake and a fake library | Link one or the other. Delete local copies of fakes that a fake library already has |
| `multiple definition of 'fff'` | Link | `DEFINE_FFF_GLOBALS` in a test or fake file | Remove it. It lives in `hal_fakes.c` only |

### Adding a HAL fake

One line in `HAL_FAKE_LIST` in `fakes/hal_fakes.h`:

```c
    VALUE(HAL_StatusTypeDef, HAL_FLASH_Unlock)                                               \
```

`VALUE(return type, name, argument types...)` for functions that return
something, `VOID(name, argument types...)` for those that don't. That one line
declares the fake, defines it, and adds it to `hal_fakes_reset()`.

### Adding a function to an existing fake library

One line in that library's list, e.g. `FAKE_CAN_MANAGEMENT_LIST` in
`fakes/fake_can_management.h`. Same `VALUE`/`VOID` format.

### Writing a new fake library

Copy the pattern of `fakes/fake_soc_flash_storage.{h,c}`:

1. `fakes/fake_<module>.h` includes the real `<module>.h` (so the compiler
   checks your fake signatures against the real ones), then defines
   `FAKE_<MODULE>_LIST(VALUE, VOID)` with one line per public function, and
   declares `void fake_<module>_reset(void);`.
2. `fakes/fake_<module>.c` expands the list with `DEFINE_FAKE_VALUE_FUNC` /
   `DEFINE_FAKE_VOID_FUNC`, defines the module's global *variables* (FFF can't
   fake data), and writes the reset, which also zeroes those globals.
   Never put `DEFINE_FFF_GLOBALS` there.
3. Add a comment at the top saying which tests it's for, and which functions
   need a `custom_fake` to be useful.
4. Use it with `FAKES <module>`. `needs_fakes.py` picks it up automatically.

FFF pastes each argument type in front of a name, so a type like
`SOC_Estimate (*)[CELLS_PER_MOD]` needs a `typedef` first. See
`fake_can_management.h`.

### A one-off local fake

For a single function that no other test needs, and that doesn't belong to a
firmware module, a local fake in the test file is fine. Reset it yourself:

```c
FAKE_VALUE_FUNC(int, sensor_read, uint8_t, float*);

void setUp(void) { RESET_FAKE(sensor_read); }
```

If a second test file needs the same fake, move it into a fake library.

### Defines

Some firmware only compiles target code when a macro is set (for example,
`soc_flash_storage.c` checks `STM32G474xx`). Pass it with `DEFINES`, and to the
tool with `-D`. (This example will only build once the flash HAL types and
functions are added to the fakes; the tool lists them.)

```cmake
add_fw_test(soc_flash_storage SOURCES kalman_soc FAKES battery_model DEFINES STM32G474xx STM32G4)
```

```sh
python Tests/tools/needs_fakes.py soc_flash_storage --with kalman_soc -D STM32G474xx -D STM32G4
```

## 9. When a test finds a bug

The BSM test from section 5 gives:

```
test_bsm.c:54:test_init_starts_in_start_dis_with_relays_open:PASS
...
test_bsm.c:123:test_fault_is_latched:PASS
test_bsm.c:141:test_ir_minus_timeout_survives_tick_wraparound:FAIL: Expected 1 Was 15

-----------------------
9 Tests 1 Failures 0 Ignored
FAIL
```

The format is `file:line:test:FAIL: expected, actual`, where `line` is the
failing assertion. Expected state 1 (`ir_minus_close`), got 15 (`FAULT`). The
BSM faulted 50 ms after closing IR−, because `HAL_GetTick() > (me->timer +
IR_FAULT_TIME_MS)` overflows when `timer` is near `UINT32_MAX`. That happens
once every 49.7 days of uptime. The wrap-safe form is `(uint32_t)(HAL_GetTick()
- me->timer) > IR_FAULT_TIME_MS`, because unsigned subtraction wraps the right
way ([CERT INT30-C](https://wiki.sei.cmu.edu/confluence/display/c/INT30-C.+Ensure+that+unsigned+integer+operations+do+not+wrap)).

When a test fails, decide which side is wrong:

1. **The code.** Fix it and keep the test, so the bug can't come back.
2. **The test** (wrong expectation, missing setup). Fix the test.
3. **The code, but it can't be fixed yet.** Don't delete the test, and
   don't leave the suite red without a word. Mark it, so it shows in every
   run:

   ```c
   void test_ir_minus_timeout_survives_tick_wraparound(void) {
       TEST_IGNORE_MESSAGE("BSM timeouts break at tick wraparound, see TO_TEST.md");
       ...
   }
   ```

   Remove the ignore when the fix lands. Agree on this with the team first:
   an ignored test is easy to forget.

Writing the failing test *before* the fix also proves the test can fail.

## 10. Static functions and static state

Firmware hides things with `static`. Ways to deal with it, best first:

1. **Test through the public function.** Static `binary_search_lut()` is
   covered by testing `BatteryModel_GetOcv()`, and the test survives
   refactoring.
2. **Capture it through a fake.** The ADC test gets the static DMA buffer
   from the fake: `HAL_ADC_Start_DMA_fake.arg1_val` is the pointer the
   firmware handed to the "hardware".
3. **A function with no header declaration** (`temp_convert` in
   `adc_management.c`): declare it at the top of the test file.
4. **Static state that persists between tests** (`comm_miss_count` in
   `adBms_Application.c`): prefer adding a reset function to the module.
   Otherwise, `#include "adBms_Application.c"` in the test file to reach its
   statics, and register the test with `NO_DEFAULT_SOURCE` so it isn't
   compiled twice.

## 11. Testing data shared with an interrupt

`support/isr_sim.h` runs a function on a background thread over and over, as
an interrupt that keeps firing. The test's own thread plays the main loop
(or an RTOS task).

The masking rules of a single-core Cortex-M apply. While the main thread
has called `__disable_irq()` (or `__set_PRIMASK(1)`, or `isr_sim_mask()`), the
simulated ISR can't run. `isr_sim_install_irq_fakes()` wires the CMSIS fakes
to the simulator. Call it inside the test, after the runner's reset. For an
RTOS critical section, point its fake's `custom_fake` at `isr_sim_mask` /
`isr_sim_unmask`.

The pattern for any shared data:

1. Pick two inputs that give very different outputs (A and B).
2. Run the ISR once with each and save the whole expected output for both.
3. `isr_sim_start(isr)`, with the ISR alternating A and B.
4. Read in a loop the way the consumer does. Every read must equal A or B
   exactly. Anything else is a torn read.
5. `isr_sim_stop()` **before** any assertion. A failed assertion jumps out of
   the test and would leave the thread running.
6. Assert the threads really overlapped (the ISR ran many times, and both A
   and B were seen). Otherwise a pass proves nothing.

`test_main_loop_never_reads_half_updated_adc_data` in `test_adc_management.c`
is the worked example. It **fails on the current firmware, on purpose**: the
DMA interrupt writes `adc_data` one field at a time while the main loop reads
it one field at a time, so the main loop sees fields from two samples about
39% of the time. `bsm_run` compares `ts_vsense` with `bat_vsense` to end
precharge, so a mixed pair matters. When the firmware is fixed (a critical
section, a double buffer or a queue), change only `main_loop_read_adc()` in
the test to read the data the new way. The check itself stays the same.

**Limits.** This is a stress test, not a proof. A pass means no torn read
showed up in the iterations run. On the board an ISR runs to completion while
the main loop is frozen. Here both threads run at once on separate cores, so
overlaps happen *more* often than on the target. It errs toward catching
problems.

## 12. Debugging a failing test

- **Run one test executable** with `ctest --test-dir Tests/build -R bsm
  --output-on-failure`, or run `Tests/build/test_bsm` directly to see every
  PASS/FAIL line.
- **Add context** with the `_MESSAGE` assertions or `TEST_MESSAGE("...")`.
- **Use a debugger.** Tests are normal PC programs. Configure with
  `-DCMAKE_BUILD_TYPE=Debug`, then `gdb Tests/build/test_bsm`, `break
  bsm_run`, `run`. `p HAL_GetTick_fake` prints a fake's counts and history. In
  VS Code, use a `cppdbg` launch configuration
  ([guide](https://code.visualstudio.com/docs/cpp/cpp-debug)).
- **Isolate.** Temporarily rename the other tests (e.g. `void
  xtest_foo(void)`) so they aren't picked up, and check whether the failure
  depends on test order.
- **Look at the generated runner** in `Tests/build/runners/` if you're unsure
  what runs, and in what order.

## 13. Common mistakes

| Symptom | Cause | Fix |
|---|---|---|
| A test never runs | Not at column 0, not `void test_<name>(void)`, or a `static` | Write it exactly as `void test_<name>(void) {` at the start of a line |
| `CMake Error: ... defines main()` | Leftover hand-written `main()` | Delete it. The runner is generated |
| `CMake Error: ... no test functions found` | The file has no `void test_...(void)` at column 0 | Add one |
| `CMake Error: no firmware source named x.c` | Typo, or the module isn't in a firmware folder | Check the name (case matters), or use `NO_DEFAULT_SOURCE` + `SOURCES` |
| Counts too high, or tests fail only when run together | A local fake isn't reset, or module state carries over | `RESET_FAKE` local fakes in `setUp()`; reset module globals there too |
| A `custom_fake` "doesn't run" | Set in a local fake before your own `RESET_FAKE` | Set it after the reset |
| Checked data looks like garbage | Pointer argument to a dead stack buffer | Copy it in a `custom_fake` (7.7) |
| `argN_history[60]` is zero | Past the 50-call history | Check `arg_histories_dropped` |
| The test hangs | Firmware waits on a register or flag that never changes | Set it before the call, or flip it in a `custom_fake` (see `test_init_reports_error_when_adc2_never_ready`) |
| Code after `Error_Handler()` keeps running | On the board it loops forever; here it's a fake that returns | Expect that, or give it a `custom_fake` that ends the wait |
| Passes on PC, fails on the board | Host and target differ: 64-bit pointers (`(uint32_t)&buffer` loses half the address), no real timing | Don't test pointer-to-`uint32_t` values; drive time through `HAL_GetTick` |
| `-Werror` fails in your test | A warning in test code | Fix it. Firmware warnings stay warnings |

## 14. Sanitizers, -Werror, coverage and CI

`Tests/cmake/quality.cmake` adds three switches, set at configure time with
`-D<option>=ON|OFF`:

| Option | Default | What it does |
|---|---|---|
| `TB_TESTS_SANITIZE` | ON where supported (not Windows) | AddressSanitizer + UndefinedBehaviorSanitizer: out-of-bounds reads, use-after-free, overflow, bad shifts. A finding fails the test |
| `TB_TESTS_WERROR` | ON | Warnings in test, fake and support code are errors. Firmware warnings stay warnings |
| `TB_TESTS_COVERAGE` | OFF | Builds with `--coverage` and adds a `coverage` target |

Coverage (gcc and `pip install gcovr`; not with zig):

```sh
cmake -S Tests -B Tests/build-cov -DTB_TESTS_COVERAGE=ON -DTB_TESTS_SANITIZE=OFF
cmake --build Tests/build-cov --target coverage
# open Tests/build-cov/coverage/index.html
```

It shows which firmware lines and branches no test reaches. That's a good
place to look for the next test.

**CI.** `.github/workflows/unit-tests.yml` runs on every push and pull request
that touches `High_Voltage/Tractive_Battery`. One job builds with gcc,
sanitizers and `-Werror` and runs ctest. The other uploads the coverage report
as an artifact. A new `add_fw_test` line is picked up with no workflow change.
While the ADC race test fails (section 11), the test job is red. That's on
purpose, so the bug stays visible.

## 15. References

Start with the first four. Come back to the rest when a topic comes up.

### Getting started

- [Memfault: Unit testing basics](https://interrupt.memfault.com/blog/unit-testing-basics):
  the best short intro to unit testing embedded C, with a CppUTest example.
- [Memfault: Mocking and fakes](https://interrupt.memfault.com/blog/unit-test-mocking):
  when to stub, fake or mock, and link seams in practice.
- [FFF repository and README](https://github.com/meekrosoft/fff): the main
  FFF documentation. Every feature used here, plus custom-fake details.
  See also its [driver testing example](https://github.com/meekrosoft/fff/tree/master/examples/driver_testing).
- [Unity: getting started](https://www.throwtheswitch.org/getting-started-with-unity):
  the project's own introduction to writing and running tests.

### Unity documentation (v2.7.0, the version in `vendor/`)

- [Docs folder](https://github.com/ThrowTheSwitch/Unity/tree/v2.7.0/docs): all
  of the guides below.
- [UnityAssertionsReference.md](https://github.com/ThrowTheSwitch/Unity/blob/v2.7.0/docs/UnityAssertionsReference.md):
  every assertion, including arrays, bit masks, floats and `_MESSAGE` variants.
- [UnityConfigurationGuide.md](https://github.com/ThrowTheSwitch/Unity/blob/v2.7.0/docs/UnityConfigurationGuide.md):
  options like `UNITY_INCLUDE_DOUBLE` and `UNITY_SUPPORT_64` (both set in our
  CMake), and float precision.
- [UnityGettingStartedGuide.md](https://github.com/ThrowTheSwitch/Unity/blob/v2.7.0/docs/UnityGettingStartedGuide.md):
  `setUp`/`tearDown`, `RUN_TEST` and what the output means.
- [UnityHelperScriptsGuide.md](https://github.com/ThrowTheSwitch/Unity/blob/v2.7.0/docs/UnityHelperScriptsGuide.md):
  Unity's own runner generator (ours is the CMake version of the same idea)
  and `TEST_CASE` for running one test over a table of inputs.

### How other projects set it up

- [Unity + CMake integration](https://embedded-focus.com/blog/cmake-unity-integration/):
  a walkthrough of a CMake build for Unity tests, close to ours.
- [Purdue Electric Racing's tests](https://github.com/PurdueElectricRacing/monorepo/tree/master/tests):
  another FSAE team's firmware tests, using GoogleTest, with CI that runs
  sanitizers and coverage.
- [CMock](https://github.com/ThrowTheSwitch/CMock) and
  [Ceedling](https://github.com/ThrowTheSwitch/Ceedling): the generated-mock
  alternative to FFF, and a build system around Unity.

### Designing good tests

| Topic | Read |
|---|---|
| Stubs, spies, mocks and fakes | [TestDouble](https://martinfowler.com/bliki/TestDouble.html), [Mocks Aren't Stubs](https://martinfowler.com/articles/mocksArentStubs.html) |
| Test behaviour, not implementation | [Software Engineering at Google, ch. 12](https://abseil.io/resources/swe-book/html/ch12.html) |
| Don't overuse fakes: link the real `battery_model.c` when you can | [SWE at Google, ch. 13](https://abseil.io/resources/swe-book/html/ch13.html) |
| Structure and naming | [ArrangeActAssert](http://wiki.c2.com/?ArrangeActAssert), [GivenWhenThen](https://martinfowler.com/bliki/GivenWhenThen.html), [naming](https://osherove.com/blog/2005/4/3/naming-standards-for-unit-tests.html) |
| Readable over DRY | [Tests too DRY](https://testing.googleblog.com/2019/12/testing-on-toilet-tests-too-dry-make.html) |
| Fixtures and isolation | [Fresh Fixture](http://xunitpatterns.com/Fresh%20Fixture.html), [xUnit Test Patterns](http://xunitpatterns.com/) |
| Choosing inputs: classes and edges | [Equivalence partitioning](https://en.wikipedia.org/wiki/Equivalence_partitioning), [boundary values](https://en.wikipedia.org/wiki/Boundary-value_analysis) |
| State machines (`bsm.c`) | [State transition table](https://en.wikipedia.org/wiki/State_transition_table) |
| A test for every bug, written before the fix | [Regression testing](https://en.wikipedia.org/wiki/Regression_testing), [TDD](https://martinfowler.com/bliki/TestDrivenDevelopment.html) |
| Pinning today's output before a refactor (`KalmanSOC_Update`) | [Characterization test](https://en.wikipedia.org/wiki/Characterization_test) |
| What unit tests can't cover | [Test pyramid](https://martinfowler.com/articles/practical-test-pyramid.html), [HIL](https://en.wikipedia.org/wiki/Hardware-in-the-loop_simulation) |
| Flaky tests | [Flaky tests at Google](https://testing.googleblog.com/2016/05/flaky-tests-at-google-and-how-we.html) |

### C and embedded pitfalls worth a test

| Topic | Read |
|---|---|
| Unsigned wraparound (`HAL_GetTick()` timeouts) | [CERT INT30-C](https://wiki.sei.cmu.edu/confluence/display/c/INT30-C.+Ensure+that+unsigned+integer+operations+do+not+wrap) |
| Float to integer (`clamp_u8` with NaN) | [CERT FLP34-C](https://wiki.sei.cmu.edu/confluence/display/c/FLP34-C.+Ensure+that+floating-point+conversions+are+within+range+of+the+new+type) |
| Comparing floats (`TEST_ASSERT_FLOAT_WITHIN` tolerances) | [Comparing floats](https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/) |
| Host vs. target sizes | [Arithmetic types](https://en.cppreference.com/w/c/language/arithmetic_types), [stdint.h](https://en.cppreference.com/w/c/types/integer) |
| Byte order in CAN payloads | [Endianness](https://en.wikipedia.org/wiki/Endianness) |
| Out-of-bounds and uninitialised reads | [CERT ARR30-C](https://wiki.sei.cmu.edu/confluence/display/c/ARR30-C.+Do+not+form+or+use+out-of-bounds+pointers+or+array+subscripts), [CERT EXP33-C](https://wiki.sei.cmu.edu/confluence/display/c/EXP33-C.+Do+not+read+uninitialized+memory) |
| Interrupts and shared data | [volatile](https://en.cppreference.com/w/c/language/volatile), [Cortex-M exceptions](https://interrupt.memfault.com/blog/arm-cortex-m-exceptions-and-nvic) |

### Tools

| Topic | Read |
|---|---|
| CTest filters, repeats, timeouts | [ctest](https://cmake.org/cmake/help/latest/manual/ctest.1.html) |
| Sanitizers | [ASan](https://clang.llvm.org/docs/AddressSanitizer.html), [UBSan](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html), [GCC options](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html) |
| Coverage | [gcov](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html), [gcovr](https://gcovr.com/en/stable/) |
| Seeing what an FFF macro expands to | [`gcc -E`](https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html), [variadic macros](https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html) |
| Weak HAL callbacks (`HAL_ADC_ConvCpltCallback`) | [GCC function attributes](https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html) |
| Link seams | [Feathers, "Seams"](https://www.informit.com/articles/article.aspx?p=359417&seqNum=3) |

### Books

- James Grenning, *[Test-Driven Development for Embedded C](https://pragprog.com/titles/jgade/test-driven-development-for-embedded-c/)*:
  the standard book on testing C firmware on a PC.
- *[Software Engineering at Google](https://abseil.io/resources/swe-book/html/ch11.html)*,
  chapters 11–13 (free online).

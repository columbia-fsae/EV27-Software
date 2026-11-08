# Test workflow

Unit tests for the Tractive Battery firmware. Run all commands from
`High_Voltage/Tractive_Battery`. Open tasks are GitHub issues labelled
[`test`](https://github.com/columbia-fsae/EV27-Software/issues?q=is%3Aopen+label%3Atest);
start with `good first issue`.

## Requirements

- CMake 3.22+
- Python 3
- A host C compiler (gcc or clang, not `arm-none-eabi-gcc`)

```sh
sudo apt install build-essential cmake ninja-build python3
```

<details>
<summary>Windows (zig via pip)</summary>

In Git Bash:

```sh
py -3 -m venv Tests/.venv-tests
Tests/.venv-tests/Scripts/python -m pip install ziglang cmake ninja
```

Create two files next to it. CMake needs an archiver, and zig only has one as
a subcommand.

```bat
:: Tests/.venv-tests/zig-ar.cmd
@"%~dp0Lib\site-packages\ziglang\zig.exe" ar %*
```

```bat
:: Tests/.venv-tests/zig-ranlib.cmd
@"%~dp0Lib\site-packages\ziglang\zig.exe" ranlib %*
```

Activate the venv (do this in every new shell, so `cmake`, `ctest` and
`python` are found), then configure once with zig:

```sh
source Tests/.venv-tests/Scripts/activate
V="$PWD/Tests/.venv-tests"
CC="$V/Lib/site-packages/ziglang/zig.exe cc" cmake -S Tests -B Tests/build -G Ninja \
    -DCMAKE_AR="$V/zig-ar.cmd" -DCMAKE_RANLIB="$V/zig-ranlib.cmd"
```

After this, the commands below work as written.

</details>

## Build and run

```sh
cmake -S Tests -B Tests/build
cmake --build Tests/build
ctest --test-dir Tests/build --output-on-failure
```

> The ADC race test fails on purpose. Every other test should pass.

<details>
<summary>Add a test to an existing test file</summary>

Add a `void test_<name>(void)` function in `Tests/test_<module>.c`,
then build and run:

```sh
cmake --build Tests/build
ctest --test-dir Tests/build -R <module> --output-on-failure
```

The build finds the new function by itself, so `Tests/CMakeLists.txt` doesn't
change. Still make the test fail once (step 5 below).

</details>

## Add a test for a new module

This example adds a test for `ADBMS6830/program/src/battery_model.c`. Swap in
your module's name: the `.c` file's name without `.c`.

**1. Check what the module needs.**

```sh
python Tests/tools/needs_fakes.py battery_model
```

```
What battery_model needs from other code:

  (none)

Suggested CMake line (Tests/CMakeLists.txt):

    add_fw_test(battery_model)
```

If the tool lists anything as `MISSING` or `real firmware`, or reports a
compile error, fix that first. See
[When the tool needs more fakes](#when-the-tool-needs-more-fakes).

**2. Write the test file**, `Tests/test_battery_model.c`:

```c
#include "battery_model.h"
#include "unity.h"

void test_lookup_table_passes_its_own_check(void) { TEST_ASSERT_TRUE(BatteryModel_VerifyTable()); }

void test_soc_below_zero_returns_the_empty_ocv(void) {
    float empty_ocv;
    TEST_ASSERT_TRUE(BatteryModel_GetTablePoint(0, NULL, &empty_ocv, NULL));

    TEST_ASSERT_EQUAL_FLOAT(empty_ocv, BatteryModel_GetOcv(-0.5f));
}
```

Each test is a `void test_<name>(void)` function starting at column 0. Don't
write `main()`: the build generates it.

<details>
<summary>If the tool suggested <code>FAKES</code></summary>

`needs_fakes.py kalman_soc` suggests `add_fw_test(kalman_soc FAKES battery_model)`.
That links `fakes/fake_battery_model.c` in place of the real module, so the
test controls what each `BatteryModel_*` call returns and can check how it was
called. HAL functions work the same way through `hal_fakes.h`. All fakes are
reset before every test.

```c
#include "fake_battery_model.h"
#include "kalman_soc.h"
#include "unity.h"

static KalmanSOC kf;

// Runs before every test. Put the module in a known state here.
void setUp(void) { KalmanSOC_Init(&kf, 0.5f); }

void test_init_from_voltage_looks_up_soc_from_ocv(void) {
    BatteryModel_GetSocFromOcv_fake.return_val = 0.8f;  // what the fake returns

    KalmanSOC_InitFromVoltage(&kf, 3.9f, 25.0f);

    TEST_ASSERT_EQUAL_INT(1, BatteryModel_GetSocFromOcv_fake.call_count);
    TEST_ASSERT_EQUAL_FLOAT(3.9f, BatteryModel_GetSocFromOcv_fake.arg0_val);
    TEST_ASSERT_EQUAL_FLOAT(0.8f, kf.x.soc);
}
```

</details>

**3. Register it** by pasting the suggested line into the Tests section at the
bottom of `Tests/CMakeLists.txt`:

```cmake
add_fw_test(battery_model)
```

**4. Configure, build and run.**

```sh
cmake -S Tests -B Tests/build
cmake --build Tests/build
ctest --test-dir Tests/build -R battery_model --output-on-failure
```

```
100% tests passed out of 1
```

**5. Make it fail once.** Break the code the test covers, rebuild, and check
the test fails. Then undo it. A test that can't fail proves nothing. Here,
change the lower clamp in `BatteryModel_GetOcv` to return `g_ocv_lut[1].ocv`:

```
test_battery_model.c:10:test_soc_below_zero_returns_the_empty_ocv:FAIL: Expected 2.77999997 Was 3.20692897
```

## When the tool needs more fakes

Fix one thing at a time and rerun `needs_fakes.py` until it exits 0.

**Compile error (exit 2).** The firmware uses a HAL type, macro, register bit
or struct field the test build doesn't have. Find it under `Drivers/` and copy
it into `fakes/stm32g4xx_hal.h`, adding only the fields the firmware uses. CubeMX
pin names (`<Pin>_Pin`, `<Pin>_GPIO_Port`) come from `Core/Inc/main.h` and go
in `fakes/main.h`. If the error is `call to undeclared function 'HAL_...'`, the
tool prints a fake line to paste, as described next.

**`MISSING` function.** The tool prints the line and the list it goes in:

```
  VALUE(HAL_StatusTypeDef, HAL_FLASH_Unlock)
      -> HAL_FAKE_LIST in Tests/fakes/hal_fakes.h  (declared in Drivers/...)
```

Paste it into that list. Every line except the last ends in `\`. The format is
`VALUE(return type, name, argument types...)` or `VOID(name, argument
types...)`, and that one line declares, defines and resets the fake. The tool
builds the line from the header with a regex, so compare it with the
prototype. Function-pointer and array arguments need a typedef (see
`fake_soc_rows_t` in `fakes/fake_can_management.h`). If the list it names is in
a `fakes/fake_<x>.h` that doesn't exist yet, write that fake first (see
[Write a new fake](#write-a-new-fake)).

**`MISSING` global variable.** Define it in `fakes/fake_<x>.c` and zero it in
`fake_<x>_reset()`.

**`no declaration found`.** No header declares the function. Search for it
with `grep -rn <name> .` and write the fake line by hand from its definition.

**`real firmware`.** Another firmware module provides the symbol. Pick one:

- **Link the real module** with `SOURCES <module>`, then rerun with
  `--with <module>` to see what that module needs in turn. Good for plain logic
  with no hardware, like `battery_model`.
- **Fake it** with `FAKES <module>` when the test should control what the
  calls return, check how they were called, or avoid hardware the real module
  touches. This needs `fakes/fake_<module>.{h,c}`.

A module can be in `SOURCES` or `FAKES`, not both.

### Write a new fake

Copy `fakes/fake_soc_flash_storage.{h,c}` if the module has only functions, or
`fakes/fake_can_management.{h,c}` if it also owns global variables. Rename the
copies `fake_<module>`, where `<module>` is the firmware `.c` file's name.

- **`.h`**: include the module's header and `fff.h`, list every public function
  in `FAKE_<MODULE>_LIST`, and declare `void fake_<module>_reset(void);`. Say
  in the top comment when to use the fake and which functions need a
  `custom_fake`.
- **`.c`**: define the list, define the module's globals, and in
  `fake_<module>_reset()` reset every fake and zero every global.

The reset function must be named exactly `fake_<module>_reset`, because the
generated runner calls it before every test. Don't add `DEFINE_FFF_GLOBALS`; it
lives in `hal_fakes.c`.

### When a return value isn't enough

A fake stores pointer arguments as pointers, not as the data they point to.
If the firmware reads results through an output pointer, or passes a buffer on
its stack, give the fake a `custom_fake` that does the work:

```c
static KalmanSOC_PersistentState saved;

static bool load_saved(KalmanSOC_PersistentState* out) {
    *out = saved;
    return true;
}

void test_restores_the_saved_soc(void) {
    SOC_Flash_Load_fake.custom_fake = load_saved;
    // ...
}
```

The top comment of each fake header lists the functions that need one.
`custom_fake` is cleared before every test. A test that hangs is usually stuck
waiting on a flag, a register or `HAL_GetTick()`. Set the value before the
call, or change it in a `custom_fake`.

### State in the module under test

The harness resets the fakes, not the module's own globals and `static`
variables. Put the module in a known state in `setUp()` by calling its init
function or setting its globals. If a test passes alone but fails with the
others, this is usually why.

## Before you push

- The new test failed once when you broke the code on purpose.
- `ctest --test-dir Tests/build --output-on-failure` shows no new failures.
- No `main()`, no `DEFINE_FFF_GLOBALS`, no copies of fakes that already exist
  in `fakes/`.
- The test file and its `add_fw_test` line are in the same commit.
- `pre-commit run --files <your files>` passes (clang-format, whitespace).

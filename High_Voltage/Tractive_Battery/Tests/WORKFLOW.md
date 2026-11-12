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

> Every test should pass. The ADC race test fails on purpose, so it is labelled
> `race` and shows as skipped. Run it with
> `TB_TESTS_OPT_IN=1 ctest --test-dir Tests/build -L race --output-on-failure`.

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

Each test function is its own ctest test, named `<module>.<name>`. To run one:

```sh
ctest --test-dir Tests/build -R '^<module>\.<name>$' --output-on-failure
Tests/build/test_<module> <name>      # or run the executable directly
Tests/build/test_<module> --list      # prints the test names
```

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
    // Below 0% SOC clamps to the table's 0% point: 2.78 V (OCV_LUT_DATA)
    const float empty_ocv = 2.78f;

    TEST_ASSERT_EQUAL_FLOAT(empty_ocv, BatteryModel_GetOcv(-0.5f));
}
```

Each test is a `void test_<name>(void)` function starting at column 0. Don't
write `main()`: the build generates it.

<details>
<summary>If the tool suggested <code>FAKES</code></summary>

The tool suggests `FAKES <module>` whenever a fake exists, but a fake is not
always the right choice. `fakes/fake_<module>.c` replaces the real module, so
the test decides what each call returns.

- **Fake it** when the real module touches hardware (flash, SPI, CAN, the HAL),
  or the test needs a result the real one can't easily give, like empty or
  corrupt flash. `adBms_Application` uses `FAKES soc_flash_storage` for this
  (see [When a return value isn't enough](#when-a-return-value-isnt-enough)).
- **Link the real module** when it's plain logic. `needs_fakes.py kalman_soc`
  suggests `FAKES battery_model`, but `battery_model` is just a lookup table,
  so rerun with `--with battery_model` and use
  `add_fw_test(kalman_soc SOURCES battery_model)`. The test then checks a real
  result:

```c
#include "kalman_soc.h"
#include "unity.h"

void test_init_from_voltage_between_table_points_interpolates_soc(void) {
    KalmanSOC kf;

    // Halfway between the 50% (3.706822 V) and 60% (3.783746 V) table points
    KalmanSOC_InitFromVoltage(&kf, 3.745284f, 25.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.55f, kf.x.soc);
}
```

With a fake, check what the module did with the value it got back, not how
many times it called the fake (driver code is the exception, see
[Writing a good test](#writing-a-good-test)). HAL fakes in `hal_fakes.h` work
the same way. All fakes are reset before every test.

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
100% tests passed out of 2
```

**5. Make it fail once.** Break the code the test covers, rebuild, and check
the test fails. Then undo it. A test that can't fail proves nothing. Here,
change the lower clamp in `BatteryModel_GetOcv` to return `g_ocv_lut[1].ocv`:

```
test_battery_model.c:10:test_soc_below_zero_returns_the_empty_ocv:FAIL: Expected 2.77999997 Was 3.20692897
```

## Writing a good test

Short version of [SWE at Google, ch. 12](https://abseil.io/resources/swe-book/html/ch12.html).
More in [REFERENCES.md](REFERENCES.md#designing-good-tests).

- **One behavior per test**, named `test_<condition>_<expected result>`, e.g.
  `test_ir_minus_aux_never_closes_goes_to_fault`. Not one test per function:
  a function with three behaviors gets three tests.
- **Arrange, act, assert**, separated by blank lines: set up the inputs, make
  the one call being tested, check the result.
- **Check results, not calls.** Assert on return values, output arguments and
  globals the rest of the firmware reads, not on which functions were called.
  The exception is HAL and driver code, where the HAL calls *are* the behavior
  (register writes, CAN frames sent), so checking the fakes is right.
- **Test through the module's header.** Don't declare a private (`static` or
  unlisted) function in the test to call it. If one really needs its own
  tests, say so in the issue.
- **Inputs from config, expected values by hand.** Use config names for inputs
  and thresholds (`IR_FAULT_TIME_MS + 1`). Write expected results as literal
  numbers worked out by hand, with a comment showing how. Never compute the
  expected value with the code under test or the same formula: a wrong formula
  then agrees with itself.
- **No loops or `if`s in tests**, except a table of cases. In the loop, use the
  `_MESSAGE` assertions so a failure says which case:
  `TEST_ASSERT_EQUAL_FLOAT_MESSAGE(want[i], got, names[i])`.
- **`setUp()` is only for what every test needs.** Anything one test depends
  on goes in that test, where the reader can see it. Helpers are fine if their
  name says what they do, like `dma_transfer_complete()` in
  `test_adc_management.c`.

```c
void test_raw_count_scales_to_volts(void) {
    const uint16_t raw[ADC1_CHANNEL_COUNT] = {1000, 0, 0, 0, 0};

    dma_transfer_complete(raw);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 242.16f, adc_data.ts_vsense);  // 1000 * 0.24216
}
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
  calls return or avoid hardware the real module touches. This needs
  `fakes/fake_<module>.{h,c}`.

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

ctest runs each test in its own process, so the module's globals and `static`
variables start at their initial values in every test. Use `setUp()` for setup
that every test in the file needs, such as calling the module's init function.

Running `Tests/build/test_<module>` with no arguments runs all its tests in one
process, so state left by one test carries into the next. If a test passes in
ctest but fails there, it depends on that state.

## Before you push

- The new test failed once when you broke the code on purpose.
- `ctest --test-dir Tests/build --output-on-failure` shows no new failures.
- No `main()`, no `DEFINE_FFF_GLOBALS`, no copies of fakes that already exist
  in `fakes/`.
- The test file and its `add_fw_test` line are in the same commit.
- `pre-commit run --files <your files>` passes (clang-format, whitespace).

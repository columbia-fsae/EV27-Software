# Test workflow

How to set up, run, and add unit tests for the Tractive Battery firmware.
[TUTORIAL.md](TUTORIAL.md) explains how the pieces work, and
[REFERENCES.md](REFERENCES.md) collects further reading. Run every command here
from `High_Voltage/Tractive_Battery`.

1. [One-time setup](#1-one-time-setup)
2. [Running the tests](#2-running-the-tests)
3. [Adding a test for a new module](#3-adding-a-test-for-a-new-module)
4. [Adding a test to an existing file](#4-adding-a-test-to-an-existing-file)
5. [When something is missing](#5-when-something-is-missing)
6. [Before you push](#6-before-you-push)
7. [Debugging a failing test](#7-debugging-a-failing-test)
8. [Sanitizers, coverage and CI](#8-sanitizers-coverage-and-ci)

## 1. One-time setup

You need CMake 3.22+, Python 3, and a host C compiler (gcc or clang), not
`arm-none-eabi-gcc`.

On Linux or WSL:

```sh
sudo apt install build-essential cmake ninja-build python3
```

<details>
<summary>Windows without a C compiler (zig, via pip)</summary>

```sh
py -3 -m venv Tests/.venv-tests
Tests/.venv-tests/Scripts/python -m pip install ziglang cmake ninja
```

CMake needs an archiver, which zig only has as a subcommand. Make two
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

Sanitizers and coverage don't work with zig on Windows. CI runs them for you.
</details>

Then configure once:

```sh
cmake -S Tests -B Tests/build
```

Run it again only after you edit `Tests/CMakeLists.txt`.

## 2. Running the tests

```sh
cmake --build Tests/build
ctest --test-dir Tests/build --output-on-failure
```

ctest prints one line per test file. To see every test function, run the
executable itself (`.exe` on Windows):

```sh
Tests/build/test_adc_management
```

```
test_adc_management.c:62:test_init_calibrates_both_adcs_and_starts_dma:PASS
...
test_adc_management.c:213:test_main_loop_never_reads_half_updated_adc_data:FAIL:388874 of 1000000 reads mixed two samples ...
```

Each line is `file:line:test:result`.

**IMPORTANT: the ADC race test fails on purpose.** It found a real race in the
firmware ([TUTORIAL section 8](TUTORIAL.md#8-testing-data-shared-with-an-interrupt)).
Don't change the test to make it pass. Every other test should pass.

| Command | What it does |
|---|---|
| `ctest --test-dir Tests/build -R bsm --output-on-failure` | Only test files whose name matches `bsm` |
| `ctest --test-dir Tests/build --rerun-failed --output-on-failure` | Only what failed last time |
| `ctest --test-dir Tests/build --repeat until-fail:50 -R adc` | Hunt a flaky test |

## 3. Adding a test for a new module

The example adds a test for `ADBMS6830/program/src/battery_model.c`. Swap in
your module's name: the `.c` file's name without `.c`.

**Ask what the module needs.**

```sh
python Tests/tools/needs_fakes.py battery_model
```

```
What battery_model needs from other code:

  (none)

Suggested CMake line (Tests/CMakeLists.txt):

    add_fw_test(battery_model)
```

If the tool lists anything as `MISSING`, or exits with a compile error, go to
[section 5](#5-when-something-is-missing) first.

**Write the test file**, `Tests/test_battery_model.c`:

```c
#include "battery_model.h"
#include "unity.h"

void test_lookup_table_passes_its_own_check(void) {
    TEST_ASSERT_TRUE(BatteryModel_VerifyTable());
}

void test_ocv_below_empty_clamps_to_empty(void) {
    TEST_ASSERT_EQUAL_FLOAT(BatteryModel_GetOcv(0.0f), BatteryModel_GetOcv(-0.5f));
}
```

Each test is a `void test_<name>(void)` function starting at column 0. Don't
write `main()`: the build generates it. Add `setUp(void)` if every test needs
the module put in a known state first.

**Register it.** Paste the suggested line into the Tests section at the bottom
of `Tests/CMakeLists.txt`:

```cmake
add_fw_test(battery_model)
```

**Configure, build and run.**

```sh
cmake -S Tests -B Tests/build
cmake --build Tests/build
ctest --test-dir Tests/build -R battery_model --output-on-failure
```

```
100% tests passed out of 1
```

**Make it fail once.** Break the code on purpose (change a `<` to `<=`, or a
clamp value), rebuild, and check the test fails. Then undo it. A test that
can't fail proves nothing.

## 4. Adding a test to an existing file

1. Add a `void test_<name>(void)` function to `Tests/test_<module>.c`.
2. `cmake --build Tests/build`. The runner picks up the new function by itself.
3. Run it.

## 5. When something is missing

`needs_fakes.py` exits with:

- **0**: everything links. Use the suggested `add_fw_test` line.
- **1**: a firmware function or variable is `MISSING`. The tool prints the
  line to add and the file it goes in.
- **2**: the module didn't compile. A HAL function nobody has faked yet shows
  up here as "undeclared", and the tool prints the line to paste for it.

| Error | Fix |
|---|---|
| `unknown type name ...`, `use of undeclared identifier 'FLASH_TYPEERASE_PAGES'`, `no member named ...` | Copy the HAL type, macro or field from `Drivers/` into `fakes/stm32g4xx_hal.h`. Only add the fields the firmware touches |
| `use of undeclared identifier '<Pin>_Pin'` | Copy the CubeMX pin name from `Core/Inc/main.h` into `fakes/main.h` |
| `call to undeclared function 'HAL_...'` | Paste the line `needs_fakes.py` prints into `HAL_FAKE_LIST` in `fakes/hal_fakes.h` |
| `undefined reference` / `undefined symbol` to a firmware function | Add `FAKES <module>` or `SOURCES <module>` to `add_fw_test`, or a line to that module's fake list |
| `multiple definition` / `duplicate symbol` | The real module and its fake are both linked. Use one |
| `multiple definition of 'fff'` | Remove `DEFINE_FFF_GLOBALS`. It lives in `hal_fakes.c` only |
| `CMake Error: ... defines main()` | Delete `main()` from the test file |
| `CMake Error: ... no test functions found` | Write tests as `void test_<name>(void)` at column 0 |
| `CMake Error: no firmware source named x.c` | Check the name (case matters) |

**Fake list lines** look like this:

```c
    VALUE(HAL_StatusTypeDef, HAL_FLASH_Unlock)                                               \
    VOID(HAL_GPIO_WritePin, GPIO_TypeDef*, uint16_t, GPIO_PinState)                          \
```

`VALUE(return type, name, argument types...)` or `VOID(name, argument
types...)`. One line declares the fake, defines it and resets it before every
test.

**A new fake library** for a whole firmware module: copy
`fakes/fake_soc_flash_storage.{h,c}`, rename it to `fake_<module>`, list the
module's public functions, define its global variables in the `.c` and zero
them in the reset. Then use `FAKES <module>`. Never add `DEFINE_FFF_GLOBALS`.

**Defines.** If the module only compiles target code behind a macro, pass it
with `add_fw_test(<module> DEFINES <macro>)` and to the tool with `-D <macro>`.

## 6. Before you push

- The new test failed once when you broke the code on purpose.
- `ctest --test-dir Tests/build --output-on-failure` shows no new failures.
- No `main()`, no `DEFINE_FFF_GLOBALS`, no copies of fakes that already exist
  in `fakes/`.
- The test file and its `add_fw_test` line are in the same commit.
- `pre-commit run --files <your files>` passes (clang-format, whitespace).
- You ticked the task in [TO_TEST.md](TO_TEST.md).

## 7. Debugging a failing test

- **Read the failure line.** `test_bsm.c:113:test_x:FAIL: Expected 1 Was 15`
  gives the line of the failing assertion, then expected and actual values.
- **Run one test file directly** (`Tests/build/test_bsm`) to see every result.
- **Add context** with the `_MESSAGE` assertions, e.g.
  `TEST_ASSERT_EQUAL_INT_MESSAGE(1, x, "after reset")`.
- **Use a debugger.** Configure with `-DCMAKE_BUILD_TYPE=Debug`, then
  `gdb Tests/build/test_bsm`. `p HAL_GetTick_fake` prints a fake's counts and
  history.
- **Test passes alone but fails with the others?** A global or a local fake
  isn't reset in `setUp()`.
- **Test hangs?** The firmware is waiting on a register or flag. Set it before
  the call, or flip it in a `custom_fake`.
- **See what runs:** the generated runners are in `Tests/build/runners/`.

## 8. Sanitizers, coverage and CI

Set these at configure time with `-D<option>=ON|OFF`:

| Option | Default | What it does |
|---|---|---|
| `TB_TESTS_SANITIZE` | ON where supported (not Windows) | ASan + UBSan. Out-of-bounds reads, overflow, bad shifts fail the test |
| `TB_TESTS_WERROR` | ON | Warnings in test and fake code are errors. Firmware warnings stay warnings |
| `TB_TESTS_COVERAGE` | OFF | Adds a `coverage` target (gcc and `pip install gcovr`) |

```sh
cmake -S Tests -B Tests/build-cov -DTB_TESTS_COVERAGE=ON -DTB_TESTS_SANITIZE=OFF
cmake --build Tests/build-cov --target coverage
# open Tests/build-cov/coverage/index.html
```

CI (`.github/workflows/unit-tests.yml`) runs on every push and pull request
that touches `High_Voltage/Tractive_Battery`: one job with sanitizers and
`-Werror`, one that uploads the coverage report. New `add_fw_test` lines need
no workflow change. While the ADC race test fails, the test job is red on
purpose.

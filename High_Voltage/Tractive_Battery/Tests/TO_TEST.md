# What to test in Tractive Battery

Every test still needed to bring the Tractive Battery firmware to high code
coverage, sorted by difficulty. [TUTORIAL.md](TUTORIAL.md) explains how to
build, run and write tests.

- [Your first test (new members start here)](#your-first-test-new-members-start-here)
- [Coverage goal and scope](#coverage-goal-and-scope)
- [Tier 0: Harness work that unblocks everything else](#tier-0-harness-work-that-unblocks-everything-else)
- [Tier 1: Easy](#tier-1-easy)
- [Tier 2: Medium](#tier-2-medium)
- [Tier 3: Hard](#tier-3-hard)
- [Tier 4: Expert](#tier-4-expert)
- [FSAE rule checks](#fsae-rule-checks)
- [Coverage map](#coverage-map)

**How to use this list**

- Put your name next to a task before you start it, e.g. `- [ ] (Alex) ...`,
  so two people don't write the same test.
- **Suspected bug** marks code that looked wrong when it was read. Write the
  test so it fails on today's code, then either fix the code or mark the test
  `TEST_IGNORE_MESSAGE(...)` until the fix lands
  ([TUTORIAL section 9](TUTORIAL.md#9-when-a-test-finds-a-bug)). Don't delete a
  failing test.
- Use the names from the config headers (`IR_FAULT_TIME_MS`,
  `CELL_OV_THRESHOLD_V`...) in tests, not their current numbers, so retuning a
  value doesn't break the test.
- Tick the box when the test is merged.

**Difficulty**

| Tier | What it takes |
|---|---|
| ★ First test | One pure function, no fakes or one existing fake. An hour or two. |
| 1 Easy | One module, existing or one-line new HAL fakes, plain inputs and outputs. |
| 2 Medium | Several fakes, `custom_fake` to fill output buffers, capturing CAN frames, static state between tests. |
| 3 Hard | Faking whole firmware modules, multi-step scenarios, or a small refactor to make code reachable. |
| 4 Expert | Numerical filters, threads, sanitizers, or tests that depend on target-only behaviour. |

---

## Your first test (new members start here)

Read [TUTORIAL.md](TUTORIAL.md) first, at least sections 1–5. Then pick **one** task below. Each needs only one new
test file and one line in [CMakeLists.txt](CMakeLists.txt). Only F5 needs a
new fake, and that's one line.

The steps are the same for all of them:

1. Run `python Tests/tools/needs_fakes.py <module>`. It prints what the module
   needs and the `add_fw_test(...)` line to use.
2. Create `Tests/test_<module>.c`. Include the module's header, `hal_fakes.h`
   and `unity.h`.
3. Write each test as `void test_<what>_<expected>(void) {` starting at
   column 0. Don't write `main()`: the runner is generated from your test
   names ([cmake/runner.cmake](cmake/runner.cmake)).
4. Add the `add_fw_test(...)` line to the test list at the bottom of
   [CMakeLists.txt](CMakeLists.txt).
5. Build and run:
   `cmake -S Tests -B Tests/build && cmake --build Tests/build && ctest --test-dir Tests/build -R <module> --output-on-failure`.
6. Break the code on purpose (change a `<` to `<=`) and check that your test
   fails. Then undo it. A test that can't fail proves nothing.
7. Open a pull request. Ask for review from whoever owns the module.

### F1. Battery model lookup helpers ([battery_model.c](../ADBMS6830/program/src/battery_model.c))

The easiest file in the project: no hardware, no fakes, pure math on a table.
`add_fw_test(battery_model)`.

- [ ] `BatteryModel_GetTablePoint`: index `-1` and `OCV_LUT_SIZE` return
      false; index `0` and `OCV_LUT_SIZE - 1` return true and fill all three
      outputs; passing `NULL` for any output pointer doesn't crash.
- [ ] `BatteryModel_GetResistance` returns `r_base` unchanged for any
      temperature (it's a TODO stub; the test documents that).
- [ ] `BatteryModel_GetNearestIndex`: `soc <= 0` gives `0`, `soc >= 1` gives
      `OCV_LUT_SIZE - 1`.
- [ ] `BatteryModel_VerifyTable()` returns true for the real table.
- [ ] `BatteryModel_GetOcv(-0.1)` equals the first table OCV and
      `BatteryModel_GetOcv(1.5)` the last one (use `GetTablePoint` to read them).

### F2. Battery state machine, first steps ([bsm.c](../BSM/src/bsm.c))

[test_bsm.c](test_bsm.c) is registered (`add_fw_test(bsm)`) with the first
test. [TUTORIAL section 5](TUTORIAL.md#5-your-first-firmware-test-the-battery-state-machine)
has a larger file with helpers for controlling time. Add the tests below to
`test_bsm.c`.

- [x] `bsm_init` sets all three relay outputs false and the state to
      `start_dis`.
- [ ] `start_dis` stays put when only `sdc_ok` is true, and when only
      `bms_ok_OUT` is true. It moves to `ir_minus_close` only when both are.
- [ ] Each state's outputs after one `bsm_run` in that state, as a table:

      | State | `pc_enable` | `ir_minus_enable` | `ir_plus_enable` |
      |---|---|---|---|
      | `start_dis` | 0 | 0 | 0 |
      | `ir_minus_close` | 0 | 1 | 0 |
      | `precharge` | 1 | 1 | 0 |
      | `ir_plus_close` | 1 | 1 | 1 |
      | `delay_post_pc` | 0 | 1 | 1 |
      | `driving` | 0 | 1 | 1 |
      | `FAULT` | 0 | 0 | 0 |

### F3. CAN clamp helpers ([can_management.h](../Communications/inc/can_management.h))

`clamp_u8`, `clamp_i8`, `clamp_u16` and `clamp_i16` are `static inline` in
the header, so including it is enough and no firmware `.c` file is needed.
Name the file `test_can_clamp.c` and register it with
`add_fw_test(can_clamp NO_DEFAULT_SOURCE)`.

- [ ] Each one: a value inside the range is unchanged (truncated toward zero),
      a value below `min` returns `min`, a value above `max` returns `max`,
      and exactly `min` and exactly `max` come back unchanged.
- [ ] `clamp_u8(255.9f, 0, 255)` is 255, `clamp_u8(-0.5f, 0, 255)` is 0.

### F4. ADBMS voltage thresholds ([adBms6830ParseCreate.c](../ADBMS6830/lib/src/adBms6830ParseCreate.c))

Pure arithmetic from the ADBMS6830 datasheet:
`code = (V − 1.5) / (16 × 150 µV) + 4096`, masked to 12 bits.
`add_fw_test(adBms6830ParseCreate)`.

- [ ] `SetOverVoltageThreshold(4.25f)` is `1145` (`0x479`).
- [ ] `SetUnderVoltageThreshold(2.5f)` is `416` (`0x1A0`).
- [ ] Exactly `1.5 V` gives `0`, and the result never has bits above bit 11
      set for any input from 0 V to 5 V.

### F5. GPIO polarity ([gpio_management.c](../Communications/src/gpio_management.c))

Teaches you how to add a fake. Add one line to `HAL_FAKE_LIST` in
[fakes/hal_fakes.h](fakes/hal_fakes.h):
`VALUE(GPIO_PinState, HAL_GPIO_ReadPin, GPIO_TypeDef*, uint16_t)`, and add any
missing pin/port names to [fakes/main.h](fakes/main.h).
`add_fw_test(gpio_management)`.

- [ ] `GPIO_Read`: with every pin reading `GPIO_PIN_SET`, every field equals
      `!<NAME>_INVERTING`; with every pin `GPIO_PIN_RESET`, every field
      equals `<NAME>_INVERTING`.
- [ ] `GPIO_Read` leaves `bms_ok_OUT` alone (it's written by the BMS, not read
      from a pin).
- [ ] `GPIO_Write`: for each of the three relay outputs, `true` drives its own
      pin `SET` and `false` drives it `RESET` (respecting `*_EN_INVERTING`).
      Check the pin with `HAL_GPIO_WritePin_fake.arg1_history`.

---

## Coverage goal and scope

**Targets**

| Code | Line | Branch |
|---|---|---|
| [bsm.c](../BSM/src/bsm.c) (drives the contactors) | 100% | 100%, plus MC/DC on the precharge condition |
| Everything else in scope | ≥ 90% | ≥ 80% |

Measure with the `coverage` target (gcc and gcovr):
`cmake -S Tests -B Tests/build-cov -DTB_TESTS_COVERAGE=ON -DTB_TESTS_SANITIZE=OFF`,
then `cmake --build Tests/build-cov --target coverage`. The report lands in
`Tests/build-cov/coverage/index.html`, and CI uploads one for every push.

**In scope**: `BSM/src`, `Communications/src`, `ADBMS6830/program/src`
(except the files below) and `ADBMS6830/lib/src`.

**Out of scope** (exclude from the coverage report and say so in the report):

- `Core/` (CubeMX init code, `main()`'s infinite loop). Two small pieces are
  worth moving out and testing: `RUN_EVERY` and `Error_Handler` (Tier 3).
- `ADBMS6830/program/src/serialPrintResult.c`: ADI's debug printing, not
  called by the firmware. One smoke test is enough (Tier 1).
- The `#ifdef MBED` halves of `mcuWrapper.c`, `serialPrintResult.c`,
  `adBms6830GenericType.c` and `adBms6830ParseCreate.c`, and the `#if TIM_EN`
  code (`TIM_EN` is 0). They are never compiled for this board.
- `Drivers/`, `Middlewares/`, `Tests/vendor/`.

---

## Tier 0: Harness work that unblocks everything else

Do these before or alongside Tier 1. Most are a few lines each.

- [x] **T0.1 Generated runner.** [cmake/runner.cmake](cmake/runner.cmake)
      writes `main()` from the `test_*` functions and resets the fakes before
      each test. `test_adc_management.c` no longer has a `main()`.
- [ ] **T0.2 Add the missing HAL fakes** to `HAL_FAKE_LIST`, each a one-line
      entry, plus any types/constants in [fakes/stm32g4xx_hal.h](fakes/stm32g4xx_hal.h).
      `needs_fakes.py` prints the line for each one.
      - [x] GPIO: `HAL_GPIO_ReadPin`, plus the `GPIOx` ports and CubeMX pin names.
      - FDCAN: `HAL_FDCAN_Init`, `HAL_FDCAN_Start`,
        `HAL_FDCAN_ActivateNotification`, `HAL_FDCAN_GetRxMessage`,
        `HAL_FDCAN_GetTxFifoFreeLevel`, `HAL_FDCAN_AddMessageToTxFifoQ`.
      - SPI and timing: `HAL_SPI_Transmit`, `HAL_SPI_Receive`, `HAL_Delay`.
      - Flash: `HAL_FLASH_Unlock`, `HAL_FLASH_Lock`, `HAL_FLASHEx_Erase`,
        `HAL_FLASH_Program`.
- [x] **T0.3 Module fakes for the `FAKES` option.** `fakes/fake_<x>.{h,c}`
      exist for `can_management`, `adBms_Application`, `kalman_soc`,
      `soc_flash_storage`, `battery_model` and `adbms_driver` (the ADBMS6830
      driver library and `mcuWrapper`: `adBmsReadData`, `adBmsWriteData`,
      `adBms6830_Snap`, `adBmsWakeupIc`, `Delay_ms`...).
- [x] **T0.4 Coverage option.** `TB_TESTS_COVERAGE` in
      [cmake/quality.cmake](cmake/quality.cmake) adds `--coverage -O0` and a
      `coverage` target that runs gcovr over the firmware sources.
- [x] **T0.5 Sanitizer option.** `TB_TESTS_SANITIZE` (same file, ON by
      default where supported) adds ASan + UBSan. Several Tier 3–4 bugs only
      show up under them.
- [x] **T0.6 CI.** [unit-tests.yml](../../../.github/workflows/unit-tests.yml)
      runs the tests with sanitizers and uploads a coverage report.
      - [ ] Still to do: post a coverage summary as a PR comment.
- [x] **T0.7 [tools/needs_fakes.py](tools/needs_fakes.py)** lists what a
      module needs to link and prints the lines to add.

---

## Tier 1: Easy

### 1.1 Battery state machine: every transition ([bsm.c](../BSM/src/bsm.c))

`test_bsm.c`. Continue from F2. Work through the
[state transition table](https://en.wikipedia.org/wiki/State_transition_table):
every state × every input that can change it.

- [ ] Full happy path `start_dis → ir_minus_close → precharge →
      ir_plus_close → delay_post_pc → driving`, checking outputs in each.
- [ ] Losing `sdc_ok` **or** `bms_ok_OUT` (test each one separately) returns
      to `start_dis` from every non-FAULT state.
- [ ] IR− aux never confirms: still `ir_minus_close` at exactly
      `IR_FAULT_TIME_MS`, `FAULT` 1 ms later. Same for IR+ in `ir_plus_close`.
- [ ] `delay_post_pc`: moves to `driving` 1 ms after
      `PRECHARGE_POST_DELAY_MS`, and faults if either aux drops before that.
- [ ] `delay_post_pc` edge: if an aux drops on the same call the delay
      expires, the code goes to `driving` (the timeout is checked first) and
      only faults on the next call. Write a test that pins down whichever
      behaviour the team decides is correct.
- [ ] `driving` faults when either aux drops; stays when both hold.
- [ ] `FAULT` is terminal: nothing leaves it, including `sdc_ok` going false.
- [ ] Every transition sets `timer` to the current tick (check `bsm.timer`).

### 1.2 Precharge decision ([bsm.c:54-60](../BSM/src/bsm.c#L54-L60))

The precharge condition has four parts. For
[MC/DC](https://en.wikipedia.org/wiki/Modified_condition/decision_coverage),
write one test per part where only that part is false and the transition
doesn't happen.

- [ ] `ts` just below and just above `bat × PRECHARGE_TS_MIN_RATIO`.
- [ ] `ts` just below and just above `bat × PRECHARGE_TS_MAX_RATIO`.
- [ ] `bat` at exactly `PRECHARGE_BAT_MIN_V` (must not pass; the check is
      strictly greater) and just above.
- [ ] Window met at exactly `PRECHARGE_MIN_TIME_MS` (must not advance) and
      1 ms later (advances).
- [ ] Window never met: still `precharge` at `PRECHARGE_FAULT_TIME_MS`,
      `FAULT` 1 ms later.
- [ ] `bat_vsense = 0` and `ts_vsense = 0` (sensor dead): must not complete
      precharge.

### 1.3 Battery model, the rest ([battery_model.c](../ADBMS6830/program/src/battery_model.c))

`test_battery_model.c`, after F1.

- [ ] `GetOcv` at every table point returns that point's OCV exactly.
- [ ] `GetOcv` halfway between two points returns the average of their OCVs.
- [ ] `GetSlope`: same three checks as `GetOcv` (clamps, table points,
      midpoints).
- [ ] `GetSocFromOcv(GetOcv(s))` is within 0.01 of `s` for `s` from 0.05 to
      0.95 in steps of 0.05.
- [ ] `GetSocFromOcv` below the first OCV returns 0; result is always within
      [0, 1].
- [ ] The top of the curve is not monotonic (OCV drops near full). Check that
      `GetSocFromOcv` at and above the second-to-last table OCV (read it with
      `GetTablePoint(OCV_LUT_SIZE - 2, ...)`; the table itself is `static`)
      uses the last segment and doesn't return a value outside [0, 1].
      **Suspected bug:** that branch returns before the `[0, 1]` clamp, so it
      extrapolates past the table for OCVs beyond its end.
- [ ] `GetNearestIndex` agrees with the binary search: for each table point
      `i` (except the last), `GetNearestIndex(soc_i + tiny)` is `i`.

### 1.4 Small lookup functions in [adBms_Application.c](../ADBMS6830/program/src/adBms_Application.c)

`test_adBms_Application.c`. This file pulls in many dependencies, so this task
uses the fake libraries from T0.3:
`add_fw_test(adBms_Application FAKES adbms_driver battery_model kalman_soc soc_flash_storage)`
(this is what `needs_fakes.py adBms_Application` suggests). The tests
themselves are easy.

- [ ] `cell_to_temp_index` for 0..23: identity up to 11, then the pairing
      12–13→12, 14–15→13, 16–17→14, 18–19→15, 20+→`i − 4`.
- [ ] `lookupID`: each of the 14 board IDs in
      [adbms_config.h](../ADBMS6830/program/inc/adbms_config.h) returns
      1–14, and an unknown ID returns 0.
      **Suspected bug:** `ID7_BOTTOM` and `ID1_BOTTOM` are the same value
      (`0x0001111111`), so board 14 is reported as board 8
      ([adbms_config.h:162](../ADBMS6830/program/inc/adbms_config.h#L162)).
- [ ] `Convert_Zener_Voltage_To_Temp`: above `TEMP_FLOATING_THRESHOLD_V`
      → `TEMP_SENSOR_FAULT_C`; between the first table voltage and the
      floating threshold → `TEMP_TABLE_MIN_C`; at or below the last voltage →
      `TEMP_TABLE_MAX_C`; each exact table voltage → its temperature; output
      never decreases as voltage decreases (sweep 1.29 V–2.45 V in 1 mV steps).

### 1.5 SOC persistence checks ([kalman_soc.c](../ADBMS6830/program/src/kalman_soc.c))

`test_kalman_soc.c`. Uncomment `add_fw_test(kalman_soc SOURCES battery_model)`
in CMakeLists.

- [ ] `KalmanSOC_Init` with a negative SOC uses `KALMAN_DEFAULT_INITIAL_SOC`;
      with 0.5 uses 0.5. Covariance diagonal is `KALMAN_INITIAL_P_DIAG`,
      off-diagonal 0, `soh_est` is `KALMAN_INITIAL_SOH`, `initialized` true.
- [ ] `KalmanSOC_InitFromVoltage(v)` sets SOC to
      `BatteryModel_GetSocFromOcv(v)`.
- [ ] `GetPersistentState` then `ValidatePersistentState` passes.
- [ ] Validation fails for: wrong magic number; one flipped bit anywhere in
      the struct (loop over every byte except `crc32`); each of `soc`, `r0`,
      `q_nom`, `soh_est` just outside its `PERSIST_*` / `SOH_*` limit (fix the
      CRC with `KalmanSOC_CalculateCRC32` after changing the field so only
      the range check can fail).
- [ ] **Suspected bug:** a `NaN` in `soc`, `r0`, `q_nom` or `soh_est` passes
      validation, because every comparison with NaN is false
      ([kalman_soc.c:301](../ADBMS6830/program/src/kalman_soc.c#L301)).
      Build a state with `soc = NAN` and a correct CRC.
- [ ] CRC check value: a struct whose first 9 bytes are `"123456789"` doesn't
      give a standard check value (the CRC covers 80 bytes), so instead
      compute the expected CRC in Python
      (`zlib.crc32(bytes(...))`) for a fixed struct and hard-code it.
- [ ] `InitFromFlash` with an invalid state returns false and leaves `kf`
      untouched; with a valid one restores every field.
- [ ] `KalmanSOC_GetEstimate`, `KalmanSOC_IsHealthy`, `KalmanSOC_Reset`
      (keeps SOC, resets covariance) and `KalmanSOC_GetStats` (writes zeros).

### 1.6 SPI wrapper and wakeup ([mcuWrapper.c](../ADBMS6830/program/src/mcuWrapper.c))

`test_mcuWrapper.c`, needs the SPI/Delay fakes from T0.2. Define `hspi1` and
`huart2` in the test file (the wrapper declares them `extern`).

- [ ] `adBmsCsLow` / `adBmsCsHigh` write `CS_PIN` on `GPIO_PORT`, `RESET`
      and `SET` respectively.
- [ ] `adBmsWakeupIc(n)` toggles CS low then high `n` times with
      `Delay_ms(WAKEUP_DELAY)` after each edge (check `fff.call_history`
      order); `n = 0` does nothing.
- [ ] `spiWriteReadBytes` transmits exactly `ADBMS_CMD_HEADER_BYTES` and
      receives `size` bytes into `rx_data`.
- [ ] `spiWriteBytes` and `spiReadBytes` pass size, buffer and
      `SPI_TIME_OUT` through.

### 1.7 Flash storage error paths ([soc_flash_storage.c](../ADBMS6830/program/src/soc_flash_storage.c))

Uncomment the `soc_flash_storage` block in CMakeLists (it defines both
`STM32G474xx` and `STM32G4`, because `Save` is behind one and `Erase` behind
the other). Leave `Load` and `HasValidData` for Tier 3; they read a fixed
flash address and crash on a PC.

- [ ] `SOC_Flash_Save` happy path: unlock, one page erase with the configured
      bank/page, 11 double-word programs at consecutive addresses, lock.
- [ ] Erase fails → returns false and the flash is locked again.
- [ ] Program fails on the 1st, 6th and last write → returns false, locked,
      no further programs.
- [ ] `SOC_Flash_Erase` returns the erase status and always locks.
- [ ] Without the defines, both return false (separate executable or a
      second `add_fw_test` with `NO_DEFAULT_SOURCE`).

### 1.8 Smoke test for the printing code

- [ ] `Print_Status_To_Console` and `adBms6830_print_soc_estimate` run without
      crashing for a healthy pack, a pack with every fault flag, and a
      segment with `FAULT_FLAG_COMM` (prints the comm line and skips the
      rest). `print_soc_estimate` prints nothing when `valid` is false
      (redirect `stdout` to a temp file to check).
- [ ] One call to each `print*` function in
      [serialPrintResult.c](../ADBMS6830/program/src/serialPrintResult.c)
      with a zeroed `cell_asic` array, just to prove it doesn't crash. No
      output checks.

---

## Tier 2: Medium

### 2.1 CAN transmit encoding ([can_management.c](../Communications/src/can_management.c))

`test_can_management.c`. Capture each frame by copying it inside a
`custom_fake` for `HAL_FDCAN_AddMessageToTxFifoQ`
([TUTORIAL 7.7](TUTORIAL.md#77-pointer-arguments-copy-the-data-not-the-pointer)). Set
`HAL_FDCAN_GetTxFifoFreeLevel_fake.return_val = 1` in `setUp()`.

- [ ] `CAN_SendData`: free level 0 returns `HAL_BUSY` without queueing; else
      the header has the right ID, standard ID type, classic CAN, BRS off and
      the given DLC, and the return value is whatever the HAL returns.
- [ ] For **each** sender (`bms_can_faults`, `bms_can_stats`, `bms_can_data`,
      `bsm_can`, `bms_can_ids`, `soc_can_stats`, `soc_can_data`, `adc_can`):
      its bit in `error_info.message_send_errors` is set when `CAN_SendData`
      fails and cleared when it succeeds, and no other bit changes.
- [ ] `error_can` sends the three error bytes and `1`, and changes no error
      bits whether or not it sends.
- [ ] `bsm_can`: byte 0 is the state, byte 4 is 1 only in `FAULT`, bytes 1–3
      and 5–6 match the relay and aux fields, DLC 7.
- [ ] `adc_can`: two 12-bit values packed into 3 bytes. Round-trip
      `ts`/`bat` at 0, 1 LSB, mid-scale and full scale; values above
      4095 × `PACK_VOLTAGE_SCALE` clamp to 4095; negative clamp to 0. The four
      temperatures use `(T − TSENSE_OFFSET) / TSENSE_SCALE`, clamped.
- [ ] `bms_can_ids`: each byte is `id[1] << 4 | id[0]`; bytes 6 and 7 are
      pack temperature and average voltage, clamped.
- [ ] `bms_can_faults`: bytes 0–1 are the pack voltage little-endian in
      `CAN_PACK_V_SCALE` units; bytes 2–7 are each segment's `fault_flags`.
      **Suspected bug:** `data1[2] = fault_flags | (balancing_done << 8U)`
      stores into a `uint8_t`, so bit 8 is always lost and `balancing_done`
      never reaches the bus
      ([can_management.c:105](../Communications/src/can_management.c#L105)).
- [ ] `bms_can_stats`: min/max cell voltage and temperature and their
      indices, including ties (first index wins) and the very first and very
      last cell. **Check:** temperatures are copied into `int8_t`, so they're
      truncated, and a dead sensor (−99 °C) becomes the minimum temperature,
      then clamps to 0 on the bus.
- [ ] `soc_can_stats`: SOC, capacity and uncertainty as little-endian
      16-bit fractions. **Suspected bug:** SOC and uncertainty aren't clamped;
      `soc = 1.01` or `-0.01` wraps (or is undefined behaviour for
      negatives). Also `CAN_SOC_CAPACITY_FULLSCALE` assumes 144 cells but the
      pack has `TOTAL_CELLS` (60).
- [ ] `clamp_u8` / `clamp_u16` with `NAN`: NaN fails both comparisons and
      the cast is undefined behaviour. Decide what it should return (0?) and
      test that.

### 2.2 CAN multi-frame sequences

- [ ] `bms_can_data` called repeatedly from counters (0, 0): IDs follow
      `CAN_ID_BMS_INIT + seg × BMS_CAN_MSGS_PER_SEGMENT + {0, 1, 2}`, cells
      4/4/2 per frame with the unused bytes zero, and the counters wrap to
      (0, 0) after the last segment. Run one full cycle and check every frame.
- [ ] Same for `soc_can_data` (8/2 cells per frame,
      `SOC_CAN_MSGS_PER_SEGMENT`), including the `soc × 255` byte at 0, 0.5
      and 1.0.
- [ ] No frame ID in either sequence collides with a fixed ID
      (`CAN_ID_BMS_PACK`, `CAN_ID_BMS_STATS`, `CAN_ID_BMS_IDS`,
      `CAN_ID_SOC_INIT`).

### 2.3 CAN receive and init

- [ ] `HAL_FDCAN_RxFifo0Callback`, with a `custom_fake` for
      `HAL_FDCAN_GetRxMessage` that fills the header and data:
      - Charger frame: bit 0 of byte 0 sets `balancing_enable`; other bits
        ignored.
      - Inverter current: little-endian signed bytes 6–7 × scale; check
        `0x7FFF`, `0x8000` and `-1` (sign extension).
      - Inverter voltage: little-endian unsigned bytes 0–1 × scale.
      - Elcon current: **big-endian** bytes 2–3, signed.
      - Unknown ID: nothing changes.
      - Without `FDCAN_IT_RX_FIFO0_NEW_MESSAGE` in `RxFifo0ITs`: nothing is
        read.
      - `GetRxMessage` fails: sets `CAN_RECEPTION_ERROR`, changes no data; the
        next good frame clears the bit.
- [ ] `can_init`: `slow_CAN` true loads the charger timing, false the car
      timing (check all 8 fields). Each of `HAL_FDCAN_Init`, `Start`,
      `ActivateNotification` failing calls `Error_Handler` once and stops
      (later calls aren't made).

### 2.4 Pack fault detection ([adBms_Application.c](../ADBMS6830/program/src/adBms_Application.c))

`test_adBms_Application.c`, after T0.3. Set `IC[...]` `c_codes`, `a_codes`
and `cccrc` directly, call `Process_Board_Data()`, check `PackSegments` and
`TotalPack`.

- [ ] Code-to-voltage: `code × LSB + 1.5 V`, plus `ADBMS_END_CELL_OFFSET_V`
      on the first and last cell only; stored as truncated mV.
- [ ] OV and UV each at threshold (no fault) and one LSB beyond (fault).
      Cells at or below `CELL_FAULT_CHECK_MIN_V` are ignored today, which
      may break EV.7.3.4 d. See [R3](#r3-missing-or-interrupted-measurements-ev734-d).
- [ ] OT just above `CELL_OT_THRESHOLD_C`; a dead sensor (−99) never sets OT.
- [ ] Temperature coverage: count how many dead sensors trip
      `FAULT_FLAG_TEMP_SENSORS` (each counts as `TEMP_DROPOUT_DEAD_CELLS`)
      and test one below and at that count. The flag is set or cleared on
      **every** segment.
- [ ] A good read clears all of that segment's previous flags.
- [ ] **Suspected bug: the comm fault isn't "consecutive".**
      `comm_miss_count` is never reset after a good read
      ([adBms_Application.c:252](../ADBMS6830/program/src/adBms_Application.c#L252)),
      so bad reads add up forever. Test: bad, good, bad, good, bad must not
      set `FAULT_FLAG_COMM`. Also, the next good read clears the "latched"
      flag. `comm_miss_count` is static: add a reset function or
      `#include` the `.c` ([TUTORIAL section 10](TUTORIAL.md#10-static-functions-and-static-state)).
- [ ] Averages: a comm-dropped segment adds nothing to `totalVoltage` or
      `avgTemp` but still counts in the `TOTAL_CELLS` divisor, so pack voltage
      and average temperature drop for the 1–2 cycles before the comm fault
      latches. Dead sensors add −99 to the average. Write tests that pin down
      the intended behaviour.
- [ ] `Update_BMS_OK_Output` (through `adBms_main_run`): any flag on any
      segment drives `BMS_OK` low and `bms_ok_OUT` false; none drives it high.
- [ ] `Update_Charger_Status` reads the slow-CAN pin without the
      `SLOW_CAN_INVERTING` that `GPIO_Read` applies, so the two disagree.
      `g_charger_present` is also never read. Decide whether to delete it.

### 2.5 Cell balancing ([adBms_Application.c:132](../ADBMS6830/program/src/adBms_Application.c#L132))

- [ ] DCC mask: only cells above `min + BAL_HYSTERESIS_V` are set; cells at
      or below `BAL_VALID_CELL_MIN_MV` are ignored when finding the minimum.
- [ ] Any OT flag on a segment → its mask is 0. A comm-faulted segment is
      skipped: its ICs' `tx_cfgb.dcc` keep their previous values (balancing
      continues on stale data; decide whether it should be cleared instead).
- [ ] Master gets bits 0–13, slave gets bits 14–23 shifted down.
- [ ] `adBmsWriteData(..., WRCFGB, Config, B)` is called once at the end.
- [ ] **Suspected bug:** `check_balance` is overwritten on every pass of
      the segment loop
      ([adBms_Application.c:177-181](../ADBMS6830/program/src/adBms_Application.c#L177-L181)),
      so `balancing_done` only reflects the last segment. Test: segment 0
      unbalanced, segment 5 balanced → `balancing_done` must be false.
- [ ] **Suspected bug (memory error):** balancing writes
      `IC[seg * 2]` and `IC[seg * 2 + 1]`, up to `IC[11]`, but
      `IC` has `TOTAL_IC` (6) entries. Segments 3–5 write past the end of
      the array. `Process_Board_Data` uses `IC[seg]` (the partial-pack TODO).
      Run under ASan (T0.5), or place a guard pattern after `IC` in a test
      build. Either way the two functions must agree on the IC index.

---

## Tier 3: Hard

### 3.1 Measurement loop ([adBms_Application.c:323](../ADBMS6830/program/src/adBms_Application.c#L323))

Fake `adBmsReadData` with a `custom_fake` that sets `cccrc.cell_pec` /
`aux_pec` for a chosen group on a chosen IC.

- [ ] A bad PEC in **any one** cell group (A–F) or aux group (A–D), not only
      the last one, ends up in `cccrc.cell_pec` / `aux_pec` after the loop.
      The code comment says this already caused a real bug; this is the
      regression test.
- [ ] Commands go out in order: wakeup, snap, 6 cell reads, unsnap, ADAX, 4
      aux reads, wakeup, ADAX2, 4 redundant aux reads.
- [ ] `Process_Cell_Balancing` runs only when `can_data->balancing_enable`.

### 3.2 Init and SOC loop ([adBms_Application.c:402-569](../ADBMS6830/program/src/adBms_Application.c#L402-L569))

- [ ] `adbms_main_init`: every IC gets `refon`, `gpo`, PWM defaults and the
      OV/UV codes from F4; config A, config B, PWM A and PWM B are written;
      board IDs from `sid` go to `id[0]` (1–7) or `id[1]` (8–14); an unknown
      ID ends up as 0 in `id[0]`.
- [ ] `adBms6830_soc_init`: comm-faulted segments are skipped; other cells
      are initialised from voltage and get the RC tables. If
      `BatteryModel_VerifyTable` fails, nothing is initialised.
      Needs `SOC_Flash_Load` faked (it reads a fixed flash address).
- [ ] `adBms6830_soc_run`: pack SOC is the minimum cell SOC; capacity is the
      sum; comm-faulted segments are skipped.
- [ ] **Suspected bug:** `TotalPack.uncertainty = uncertainty / NUM_SERIES_CELLS`
      expands to `uncertainty / TOTAL_MODULES * CELLS_PER_MOD`, because the
      macro has no parentheses
      ([kalman_soc_config.h:15](../ADBMS6830/program/inc/kalman_soc_config.h#L15)).
      The result is 100× too large. Test: 60 cells each with uncertainty
      0.01 → pack uncertainty 0.01. `TOTAL_CELLS` in `adbms_config.h` has the
      same missing parentheses; today's uses happen to be safe.
- [ ] **Suspected bug:** with every segment comm-faulted, no cell updates,
      so `TotalPack.soc` stays at `SOC_MIN_SEARCH_INIT` (2), which then
      overflows in `soc_can_stats` (2.1).
- [ ] `adBms6830_soc_update` does nothing for an uninitialised cell and
      counts successful updates.
- [ ] `adBms6830_soc_save_to_flash` / `clear_flash`: one save per initialised
      cell, erase called once. (Every cell writes the same address today:
      the TODO in the code.)

### 3.3 Flash load ([soc_flash_storage.c:74](../ADBMS6830/program/src/soc_flash_storage.c#L74))

- [ ] Add a seam so tests can point the flash address at a RAM buffer, e.g.
      wrap `#define FLASH_SOC_STORAGE_ADDRESS` in `#ifndef` and pass the
      buffer address with `DEFINES`, or read through a small function.
- [ ] Then: `SOC_Flash_Load` and `SOC_Flash_HasValidData` accept a saved
      state, reject a blank page (all `0xFF`), a wrong magic number and a bad
      CRC.
- [ ] Save then Load round trip, using a `HAL_FLASH_Program` fake that writes
      into the buffer.
- [ ] **Suspected bug (memory error):** `num_dwords` rounds 84 bytes up to
      88, so `SOC_Flash_Save` reads 4 bytes past the struct. Shows up under
      ASan; or check the last programmed double-word's upper half.

### 3.4 ADBMS driver library ([adBms6830GenericType.c](../ADBMS6830/lib/src/adBms6830GenericType.c), [adBms6830ParseCreate.c](../ADBMS6830/lib/src/adBms6830ParseCreate.c))

ADI's code, but the firmware depends on it.

- [ ] PEC15 and PEC10: `Pec15_Calc`, `pec10_calc`, `adi_pec10_calc_table`
      and `CalcPEC10` agree on random data, and match the example frames in
      the ADBMS6830 datasheet.
- [ ] `spiReadData`: with a fake SPI that returns frames with correct PECs,
      `pec_error` is 0 for every IC and `cmd_cntr` is decoded; corrupt one
      byte of IC 3 → only `pec_error[3]` is 1.
- [ ] `adBmsReadData` for each `TYPE` used by the firmware (`Config`, `Cell`,
      `Aux`, `RAux`, `Sid`) calls the right parser and stores the PEC in the
      right `cccrc` field.
- [ ] **Suspected bug (memory error):** for the read-all types
      (`ALL_GRP`), `read_buffer` holds one IC's bytes but `spiReadData`
      writes `tIC` times that, and `RX_BUFFER` is a `uint8_t`, so
      `RDASALL_SIZE × 6 = 420` wraps to 164. The firmware doesn't use these
      reads today. Also `rBuff_size` is uninitialised for a wrong read-all
      type.
- [ ] `adBmsWriteData`: for config A/B, PWM A/B, comm and clear-flag, the
      bytes passed to SPI equal each IC's `tx_data` in order, with a valid
      PEC.
- [ ] Command helpers (`adBms6830_Adcv`, `Adax`, `Adax2`, `Adsv`, `Snap`,
      `Unsnap`): the two command bytes for a few argument combinations,
      checked against the datasheet.
- [ ] Parsers (`ParseCell`, `ParseAux`, `ParseRAux`, `ParseConfiga/b`,
      `ParseStatusA–E`, `ParseSID`, `ParsePwma/b`, `ParseComm`): feed known
      byte arrays, check fields and per-IC offsets. Then `Create*` functions:
      `Create(Parse(bytes)) == bytes` for config and PWM.
- [ ] `ConfigA_Flag`, `ConfigA_Gpo`, `ConfigB_DccBit`,
      `SetConfigB_DischargeTimeOutValue`, `SetPwmDutyCycle`: table tests.

### 3.5 Code in main.c worth testing

Both need a small refactor out of [main.c](../Core/Src/main.c) first.

- [ ] Move `RUN_EVERY` into a header, then test: runs at the first due tick,
      not before; the offset delays the first run; it keeps running at the
      interval; **suspected bug:** it compares `curr_tick + offset >=
      interval + tick`, which breaks when either side overflows (after
      ~49.7 days). Use the wrap-safe subtraction form.
- [ ] `Error_Handler`: **suspected bugs:** bytes 0–2 of the error CAN
      frame are sent uninitialised, and `INIT_CAN_SEND_ERROR` is set and then
      cleared on the next line, so it's always 0. The rest (reset counter,
      stuck after `MAX_SOFT_RESETS`, BSM forced to `FAULT`) needs
      `NVIC_SystemReset` faked and a way out of the `while (1)`.
- [ ] `reset_counter_init`: wrong magic resets the counter, right magic keeps
      it.

---

## Tier 4: Expert

### 4.1 Kalman filter behaviour ([kalman_soc.c:336](../ADBMS6830/program/src/kalman_soc.c#L336))

Correct output is hard to state exactly, so mix property tests with
[characterisation tests](https://en.wikipedia.org/wiki/Characterization_test)
(record today's output from the SIL model, then check the C code matches).

- [ ] Properties over long random runs (fixed seed): SOC stays in [0, 1];
      covariance stays symmetric with a positive diagonal; no NaN or Inf in
      any field.
- [ ] Zero current at rest voltage: SOC doesn't drift over 1 hour.
- [ ] Constant current for N seconds moves SOC by about `I × t / Q`.
- [ ] Innovation larger than `MAX_PHYSICAL_INNOVATION_V`: returns true
      without correcting, clears the innovation window.
- [ ] Singular innovation covariance returns false (both
      `KALMAN_MIN_DETERMINANT` checks).
- [ ] First call (`kf->t = 0` with a large timestamp) and `dt = 0`.
- [ ] Fallback RC values when the lookup tables aren't set, versus the 2D
      lookup (check `interp2d` at grid points, between points and clamped
      outside the grid).
- [ ] **Suspected bug:** the SOH update above `SOH_UPDATE_SOC_THRESHOLD`
      calls `interp2d(kf->g_r0_lut, ...)` even when the tables were never set,
      and `KalmanSOC_Init` doesn't set the table pointers to NULL. A filter
      that hasn't had `SetRCLookupTables` called crashes (or reads garbage)
      once SOC goes above 0.9.
- [ ] **Check intent:** `estimate->uncertainty` is `sqrt(P[1][1])`, the
      variance of `v_ct`, not of SOC (`P[0][0]`).
- [ ] Float timestamp precision: `timestamp_ms / 1000.0f` loses millisecond
      resolution after about 4.6 hours of uptime; at 24 h the step is about
      8 ms. Run with timestamps near 24 h and compare against a run near 0.
- [ ] Timing: measure `KalmanSOC_Update` on the PC as a rough regression
      guard. 60 cells per loop must fit the BMS period on the target; the
      real number has to come from the board.

### 4.2 Concurrency

Use [support/isr_sim.h](support/isr_sim.h) as in
the ADC race test ([TUTORIAL section 11](TUTORIAL.md#11-testing-data-shared-with-an-interrupt)).

- [x] ADC race (`test_main_loop_never_reads_half_updated_adc_data`).
- [ ] `can_data` is written by the FDCAN RX interrupt and read by the main
      loop. Check that `tractive_current` and `dc_bus_voltage` are never
      torn, and that `tractive_current` can come from two message IDs
      (inverter and Elcon) depending on which arrived last.
- [ ] `error_info` is changed with read-modify-write (`|=`, `&=`) by both the
      RX interrupt and the main loop. A bit set in one can be lost by the
      other. Show it with `isr_sim`.
- [ ] Run all `isr_sim` tests under ThreadSanitizer (Linux/WSL).

### 4.3 Whole-suite quality checks

- [ ] Run everything under ASan + UBSan (T0.5) with
      `-fsanitize=float-cast-overflow`. The CAN clamps, flash over-read,
      balancing out-of-bounds write and read-all heap overflow should all
      show up.
- [ ] Mutation testing on `bsm.c` and `can_management.c`
      ([Mull](https://github.com/mull-project/mull)): every surviving mutant
      is a missing test.
- [ ] Fuzz `adBms6830ParseCell` / `ParseAux` / `spiReadData` with random byte
      arrays (libFuzzer).

---

## FSAE rule checks

Tests that show the firmware meets a specific rule in the
[Formula SAE Rules 2026, Version 1.0 (10 Sept 2025)](https://www.fsaeonline.com/cdsweb/gen/DownloadDocument.aspx?DocumentID=278fd4d7-aa27-4e33-bc4a-090148e662a0).
Every task names the rule it checks. A test that fails here can fail
Electrical Technical Inspection, so these come before coverage work.

- Put the rule number in the test name or a comment above it, e.g.
  `// EV.7.3.3` above `test_balancing_off_when_sdc_open`.
- Re-check the rule numbers and wording when a new rulebook comes out.
- Some limits come from the cell data sheet (EV.7.4.2, EV.7.5.2). Add them
  to a config header as named constants (e.g. `CELL_DATASHEET_V_MAX`,
  `CELL_DATASHEET_T_MAX_C`) together with the ADBMS6830 and thermistor
  measurement accuracy, so the tests below compare names rather than numbers.
- Several rules are met partly in hardware: the Shutdown Circuit, its
  latch and the indicator lights. A unit test can only cover the firmware
  part. Where that applies, the task says so, and the rest needs a bench
  test.

### R1. Fault thresholds inside the legal limits (EV.7.4.2, EV.7.5.2)

> EV.7.4.2: Cell Voltage levels must stay inside the permitted minimum and
> maximum cell voltage levels stated in the cell data sheet. Measurement
> accuracy must be considered.
>
> EV.7.5.2: Temperatures (considering measurement accuracy) must stay below
> the lower of the two: the maximum cell temperature limit stated in the
> cell data sheet, 60°C

Write these as `_Static_assert`s in a test file (or in
[adbms_config.h](../ADBMS6830/program/inc/adbms_config.h) itself) so a bad
retune fails the build.

- [ ] `CELL_OV_THRESHOLD_V + V accuracy <= CELL_DATASHEET_V_MAX`.
      **Suspected rule violation:** `CELL_OV_THRESHOLD_V` is 4.25 V, but the
      firmware's own `MAX_CELL_VOLTAGE` in
      [kalman_soc_config.h:19](../ADBMS6830/program/inc/kalman_soc_config.h#L19)
      and the lap sim's `max_v` for the AMPACE JP50 are both 4.2 V. Confirm
      the maximum against the data sheet.
- [ ] `CELL_UV_THRESHOLD_V - V accuracy >= CELL_DATASHEET_V_MIN`.
- [ ] `CELL_OT_THRESHOLD_C + T accuracy < min(CELL_DATASHEET_T_MAX_C, 60)`.
      Today the threshold is exactly 60.0 with a strict `>` check, so a cell
      reading 60.0 °C doesn't fault and the rule's accuracy margin is zero.
- [ ] The first and last cell of each IC get `ADBMS_END_CELL_OFFSET_V` added
      before the check. Test OV/UV on those two cells separately, so the
      offset can't push a real overvoltage under the threshold.

### R2. Out-of-range values open the Shutdown Circuit (EV.7.3.4 a/c, EV.7.3.5)

> EV.7.3.4: The BMS must monitor for: a. Voltage values outside the
> permitted range […] c. Temperature values outside the permitted range
>
> EV.7.3.5: If the BMS detects one or more of the conditions of EV.7.3.4
> above, the BMS must: a. Open the Shutdown Circuit

Builds on [2.4](#24-pack-fault-detection-adbms_applicationc). Run the whole
chain `Process_Board_Data` → `Update_BMS_OK_Output` and check the `BMS_OK`
pin write, not just the flag.

- [ ] Every cell index: loop over all `TOTAL_MODULES × CELLS_PER_MOD`
      cells, put one cell just over OV (then just under UV) with every other
      cell normal, and check that `BMS_OK` goes low. When the slave ICs are
      enabled for the full pack, this test must still pass. It's the one that
      catches a cell nobody checks.
- [ ] Same for every temperature sensor index just over the OT threshold.
- [ ] Property test (fixed seed, about 10,000 random packs): `BMS_OK` is low
      exactly when some cell is out of range.

### R3. Missing or interrupted measurements (EV.7.3.4 d)

> EV.7.3.4: The BMS must monitor for: […] d. Missing or interrupted voltage
> or temperature measurements

- [ ] **Suspected rule violation:** a cell that reads at or below
      `CELL_FAULT_CHECK_MIN_V` (1.0 V) skips both OV and UV checks
      ([adBms_Application.c:278](../ADBMS6830/program/src/adBms_Application.c#L278)),
      so an open sense wire or missing tap gives no fault. Test: one cell
      at 0 V must drive `BMS_OK` low. If the skip is needed for bench work
      with a partial pack, put it behind a bench-only build flag and test
      that the flag is off in the release build.
- [ ] A disconnected temperature sensor (`TEMP_SENSOR_FAULT_C`) is a missing
      measurement. Today it only counts toward the coverage check in R4 and
      never faults on its own. Decide with the electrical lead whether that
      meets EV.7.3.4 d, and write the test for the decision.
- [ ] A shorted temperature sensor saturates at `TEMP_TABLE_MAX_C` and must
      trip OT. An open one must read `TEMP_SENSOR_FAULT_C`, not a plausible
      temperature.
- [ ] Comm loss: `COMM_FAULT_LATCH` consecutive PEC failures on a segment
      drive `BMS_OK` low. This depends on the `comm_miss_count` bug in 2.4
      being fixed, since today the count isn't consecutive.

### R4. Temperature coverage (EV.7.5.5)

> EV.7.5.5: For lithium based cells, a. The temperature of a minimum of 20%
> of the cells must be monitored by the BMS b. The monitored cells must be
> equally distributed inside the Tractive Battery Container(s)

- [ ] Test against the rule, not the formula. Work out the real fraction of
      cells covered by working sensors and check that
      `FAULT_FLAG_TEMP_SENSORS` is set exactly when it drops below 20%.
      Today the check is `(working fraction) × TEMP_COVERAGE_FACTOR (0.5) <
      MIN_CELL_THRESH (0.2)`, which trips at 40%. Make sure that's the margin
      the team intended and not a units mix-up.
- [ ] Distribution: dead sensors are counted across the whole pack. Kill
      every sensor in one segment while the rest work. Decide whether that
      must fault under part b (it doesn't today), and test the decision.

### R5. Fault stays until manually reset (EV.7.3.5, EV.7.2.3)

> EV.7.3.5: […] The two lights must stay on until the BMS is manually reset
>
> EV.7.2.3: When the BMS, IMD or BSPD Open the Shutdown Circuit: a. The
> Tractive System must stay disabled until manually reset

- [ ] Find out where the latch is. If it's in hardware (an SDC latch
      circuit), write that down in this task and leave the firmware as is,
      but add a bench test. If it's meant to be firmware, it isn't there
      yet: `Process_Board_Data` clears a segment's `fault_flags` on every
      good read, so `BMS_OK` goes high again as soon as the cell recovers.
      Test: OV for one cycle, then normal for many cycles. `BMS_OK` must
      stay low until the reset input or a power cycle.

### R6. No balancing with the Shutdown Circuit open (EV.7.3.3)

> EV.7.3.3: Cell balancing is not permitted when the Shutdown Circuit is
> Open

Builds on [2.5](#25-cell-balancing-adbms_applicationc132). Today balancing is
controlled only by `can_data->balancing_enable` (a bit in the charger's CAN
frame, [adBms_Application.c:390](../ADBMS6830/program/src/adBms_Application.c#L390)).
`sdc_ok` and `BMS_OK` aren't checked.

- [ ] `sdc_ok` false, `balancing_enable` true, unbalanced pack: the DCC
      bits written with `WRCFGB` must all be 0.
- [ ] Any BMS fault (which opens the Shutdown Circuit through `BMS_OK`):
      DCC must be 0, not only for OT as today.
- [ ] **Suspected rule violation:** when `balancing_enable` goes false,
      `Process_Cell_Balancing` simply stops being called. The DCC bits
      already written to the ADBMS6830s are never cleared, so the cells keep
      discharging. Test: balance one cycle, set `balancing_enable` false
      (or open the SDC), run again, and expect a `WRCFGB` write with every
      `dcc` at 0.
- [ ] `balancing_enable` keeps its last value if the charger's CAN frames
      stop. Test that losing the charger frames for longer than a timeout
      turns balancing off.

### R7. Precharge to 90% before the second IR closes (EV.5.6.1 a, EV.5.6.2)

> EV.5.6.1: The Precharge Circuit must: a. Be able to charge the
> Intermediate Circuit to minimum 90% of the Tractive System voltage before
> closing the second IR
>
> EV.5.6.2: The Intermediate Circuit must precharge before closing the
> second IR. a. The end of precharge must be controlled by feedback by
> monitoring the voltage in the Intermediate Circuit

Builds on [1.1](#11-battery-state-machine-every-transition-bsmc) and
[1.2](#12-precharge-decision-bsmc54-60).

- [ ] `_Static_assert(PRECHARGE_TS_MIN_RATIO >= 0.90 + ADC ratio accuracy)`.
      Today it's 0.905, so the margin for ADC error is 0.5%. Check that
      covers the error in the two `vsense` channels.
- [ ] Property test (fixed seed, random sequences of `sdc_ok`,
      `bms_ok_OUT`, aux contacts, `ts_vsense`, `bat_vsense` and ticks): if
      `ir_plus_enable` turns on, the state machine must have just left
      `precharge`, and the `ts_vsense` in that same call must have been at
      least `0.90 × bat_vsense`.
- [ ] Feedback, not just a timer: with `ts_vsense` stuck at 0, IR+ never
      closes however long the test runs (it ends in `FAULT`).

### R8. Shutdown Circuit open means the Tractive System shuts down (EV.7.2.2)

> EV.7.2.2: When the Shutdown Circuit Opens: a. The Tractive System must
> Shutdown b. All Tractive Battery current flow must stop immediately

The Shutdown Circuit carries the IR coil current directly (EV.7.1.2), so
the IRs open in hardware whatever the firmware does. These tests check that
the firmware doesn't fight it, so nothing re-closes when the SDC comes back.

- [ ] From every state, `sdc_ok` going false clears `pc_enable`,
      `ir_minus_enable` and `ir_plus_enable` within two `bsm_run` calls
      (one `BSM_RUN_PERIOD_MS`). Today the call that sees the drop still
      writes that state's outputs before it transitions, so they clear on
      the next call. Test that this is the only delay.
- [ ] When `sdc_ok` comes back, the sequence starts again from
      `ir_minus_close` and goes through precharge. It never jumps straight
      back to IR+ closed.

### R9. Charger turns off and stays off (EV.8.4.1, EV.8.4.2)

> EV.8.4.1: When Charging, the BMS and IMD must: a. Monitor the Tractive
> Battery b. Open the Charging Shutdown Circuit if a fault is detected
>
> EV.8.4.2: When the Charging Shutdown Circuit Opens: […] c. The Charger
> must be turned off d. The Charger must stay disabled until manually reset

This code is in the Charging Cart project
([chargersm.c](../../Charging_Cart/UI/STM32/ChargerSM/src/chargersm.c)),
which doesn't have a test harness yet. The Tractive Battery part is that
R2–R4 keep working while `balancing_enable` is set (EV.7.3.1 b).

- [ ] Any fault in `fault_case` (SDC, BSM fault, Elcon fault, timeouts)
      sets `charging` and `balancing` false in the same `chargersm_run` call.
- [ ] **Check intent:** `FAULT` goes back to `ready` by itself once the
      fault clears. Charging only restarts after the user selects a mode
      again, because `userVars[STATUS]` is set to `NO_OPTIONS`. Decide
      whether that counts as the manual reset in EV.8.4.2 d, then test that
      charging never restarts without a user action.
- [ ] `TRAINING_WHEELS_MODE` turns off every fault check. Add a build check
      (an `#error` for release builds, or a CI grep) so it can't ship
      defined.

**Not included:** these came up in review but no 2026 FSAE rule asks for
them. Keep them in the tiers above for code quality, not compliance:

- A BMS reaction-time limit. FSAE 2026 doesn't set one. Formula Student
  events using the FS Rules may, so check those if the car goes to one.
- The SOC estimate, CAN encoding against the DBC, `RUN_EVERY` tick
  wraparound, and the `BAL_HYSTERESIS_V` value (`0.05f`, but its comment
  says 5 mV).

---

## Coverage map

Where each function gets covered. If you add a function to the firmware, add
it here.

| File | Functions | Tasks |
|---|---|---|
| [bsm.c](../BSM/src/bsm.c) | `bsm_init`, `bsm_run` | F2, 1.1, 1.2, tick wrap in [TUTORIAL section 9](TUTORIAL.md#9-when-a-test-finds-a-bug) |
| [gpio_management.c](../Communications/src/gpio_management.c) | `GPIO_Read`, `GPIO_Write` | F5 |
| [adc_management.c](../Communications/src/adc_management.c) | `adc_init`, `temp_convert`, `HAL_ADC_ConvCpltCallback` | Mostly done in [test_adc_management.c](test_adc_management.c). Still to add: ADC1 and ADC2 calibration failing separately; `HAL_ADC_Start_DMA` and `HAL_DMA_Start` return values are ignored, so decide whether a failure should call `Error_Handler` and test it |
| [can_management.c](../Communications/src/can_management.c) | all 12 | F3, 2.1, 2.2, 2.3, 4.2 |
| [adBms_Application.c](../ADBMS6830/program/src/adBms_Application.c) | lookups, fault detection, balancing | 1.4, 1.8, 2.4, 2.5 |
| | `measurement_loop`, init, SOC loop, flash helpers | 3.1, 3.2 |
| [battery_model.c](../ADBMS6830/program/src/battery_model.c) | all 9 | F1, 1.3 |
| [kalman_soc.c](../ADBMS6830/program/src/kalman_soc.c) | init, persistence, helpers | 1.5 |
| | `KalmanSOC_Update`, `interp2d` | 4.1 |
| [soc_flash_storage.c](../ADBMS6830/program/src/soc_flash_storage.c) | Save, Erase | 1.7 |
| | Load, HasValidData | 3.3 |
| [mcuWrapper.c](../ADBMS6830/program/src/mcuWrapper.c) | non-MBED half | 1.6 |
| [adBms6830GenericType.c](../ADBMS6830/lib/src/adBms6830GenericType.c) | PEC, SPI read/write, commands | 3.4 |
| [adBms6830ParseCreate.c](../ADBMS6830/lib/src/adBms6830ParseCreate.c) | thresholds | F4 |
| | parsers, creators, config helpers | 3.4 |
| [serialPrintResult.c](../ADBMS6830/program/src/serialPrintResult.c) | all | 1.8 (smoke test only) |
| [main.c](../Core/Src/main.c) | `RUN_EVERY`, `Error_Handler`, `reset_counter_init` | 3.5 (rest out of scope) |

# SOC Estimator SIL (Software-in-the-Loop)

Runs the **real, unmodified** embedded SOC estimator (`kalman_soc.c` + `battery_model.c`
from the ADBMS6830 firmware) against a CSV of logged current/voltage data, compiled as a
native program and driven exactly the way the firmware drives it -- not a Python
reimplementation of the Kalman filter.

## Layout

- `c_src/` -- byte-for-byte copies of the real firmware source, untouched:
  `kalman_soc.c/.h`, `kalman_soc_config.h`, `battery_model.c/.h`, `common_types.h`
  (originals live in `High_Voltage/Tractive Battery/ADBMS6830/program/` and
  `High_Voltage/Tractive Battery/Communications/inc/`). These files have zero hardware
  dependencies (no HAL, no MCU headers) -- that's what makes native SIL possible without
  modifying them at all.
- `harness/sil_harness.c` -- **new** glue code (not a modification of the files above).
  Reproduces the exact call sequence `adBms_Application.c` uses
  (`adBms6830_soc_init`/`adBms6830_soc_update`): `KalmanSOC_InitFromVoltage()` from the
  first CSV row, `KalmanSOC_SetRCLookupTables()` with the same R/C lookup table constants
  copied verbatim from `adBms_Application.c`, then one `KalmanSOC_Update()` call per CSV
  row in order. Reads an input CSV, writes an output CSV with the estimator's results.
- `build.sh` -- compiles `c_src/*.c` + `harness/sil_harness.c` into `bin/soc_sil`.
- `generate_example_data.py` -- writes a synthetic (not real) current/voltage log to
  `example_data/synthetic_drive_cycle.csv` for smoke-testing the pipeline.
- `run_soc_sil.py` -- builds the harness if needed, runs it against a CSV, and plots the
  results (SOC%, estimated OCV vs. measured voltage, R0, SOH, RC branch states, filter
  confidence).

## Input CSV format

A header row followed by data rows with these columns (any order):

| column | meaning | units |
|---|---|---|
| `time_ms` | milliseconds since the start of the log | ms (matches `HAL_GetTick()` semantics) |
| `voltage` | **single-cell** terminal voltage | V |
| `current` | pack current, **positive = discharge** | A |
| `temperature` | optional; defaults to 25.0 C for every row if omitted | deg C |

Current/voltage are per the real firmware's own convention (`cic`/`cell`-level voltage,
pack-level current) and `kalman_soc.c`'s sign convention (SOC decreases when current is
positive) -- no conversion is applied, so make sure your log matches this before feeding
it in.

## Usage

```bash
# One-time: install a C compiler if you don't have one (WSL/Linux)
sudo apt install build-essential

pip install -r requirements.txt

# Try it with synthetic data first
python generate_example_data.py
python run_soc_sil.py

# Then with your real data
python run_soc_sil.py path/to/your_log.csv
```

`run_soc_sil.py` recompiles `bin/soc_sil` automatically whenever the C sources change;
pass `--rebuild` to force it. The compiled executable can also be run directly:

```bash
./bin/soc_sil input.csv output.csv
```

## Why this counts as SIL, not a Python port

The estimator's actual math never runs in Python. `run_soc_sil.py` only shells out to a
natively-compiled binary built from the untouched `kalman_soc.c`/`battery_model.c`, and
the harness calls the same public functions
(`KalmanSOC_InitFromVoltage`/`KalmanSOC_SetRCLookupTables`/`KalmanSOC_Update`) in the same
order the firmware does. Python's only job is generating/plotting data around that binary.

# Fuse Testing current logger

Logs fuse-test current from an ADBMS2950B (50 µΩ shunt on I1) at its native ~1 kS/s:

```
NUCLEO-G474RE --SPI1--> ADBMS6822 eval --isoSPI--> ADBMS2950B --> I1 = shunt voltage
      |
      +-- ST-LINK USB virtual COM port (1 Mbaud, framed binary) --> laptop: pc/fuse_logger.py
```

## Layout

| Path | What |
|---|---|
| `firmware/` | CMake + STM32 HAL project for the NUCLEO-G474RE (same toolchain setup as EV27 Tractive_Battery) |
| `firmware/App/inc/app_config.h` | All settings: pins, shunt value, I1 sign, I2 on/off, baud |
| `firmware/App/src/adbms2950.c` | ADBMS2950B commands, PEC, register reads |
| `firmware/App/src/logger.c` | Start-up sequence and the sampling loop |
| `firmware/App/src/stream.c` | Framed, CRC-checked, interrupt-driven UART stream |
| `pc/fuse_logger.py` | Receives the stream, live plot, saves each run, plots the finished run |
| `pc/sim_device.py` | Fake Nucleo over TCP, for testing the PC side without hardware |
| `logs/` | Test data, tracked in git: `<name>_YYYYMMDD_HHMMSS.csv`, `_events.txt`, `.png` (subfolders allowed) |

## Wiring

Same as EV27 Tractive_Battery: the ADBMS6822 board on the Nucleo's Arduino header,
SPI1 = PA5 (D13 SCK), PA6 (D12 MISO), PA7 (D11 MOSI), chip select = PB6 (D10).
The ADBMS2950B must be the only device on the isoSPI link. The shunt goes across the I1 inputs.

## Build and flash

Needs `arm-none-eabi-gcc`, CMake ≥ 3.22 and Ninja on PATH (the STM32 VS Code extension /
STM32CubeCLT provide them).

```
cd firmware
cmake --preset Release
cmake --build build/Release
```

Then flash `build/Release/Fuse_Testing.elf` with STM32CubeProgrammer or the VS Code debugger,
or drag `Fuse_Testing.bin` onto the Nucleo's USB drive.

## Run a test

```
cd pc
pip install -r requirements.txt
python fuse_logger.py            # auto-detects the ST-LINK COM port; --port COM5 to force
```

It asks for a run name (e.g. `30A_slowblow`); the files are named `<name>_<YYYYMMDD_HHMMSS>`.
A `/` in the name makes subfolders: `calibration/test1` saves
`logs/calibration/test1_<YYYYMMDD_HHMMSS>.csv`. `--name calibration/test1` skips the prompt. Each run restarts the ADBMS2950B measurement (~170 ms, waits for I1ADC calibration),
then logs until you close the live plot window or press Ctrl+C. When the run ends,
the whole run is plotted as current (A) vs time (s) and saved as a PNG next to its CSV.

```
python fuse_logger.py --plot                      # re-plot the most recent run (any subfolder)
python fuse_logger.py --plot ..\logs\<file>.csv     # or a specific one
python fuse_logger.py --no-live                   # no live window (lower CPU), Ctrl+C to stop
```

CSV columns: `t_s` (MCU time since the run's first sample), `i1_A`, `i2_A`, `i1cnt`
(the 2950B's conversion counter), `seq` (frame counter), `flags`, `gap`, raw `i1_code`/`i2_code`.
The events file has the MCU messages, any lost frames or missed conversions, and the run summary.

## How sampling works

- `ADI1` with CONT=1 (and RD=1 so I2 runs too) starts I1ADC continuously. It produces one result
  every 1 ms on the 2950B's own oscillator (0.9–1.1 kS/s per the datasheet), which is not
  synchronous to the MCU.
- The firmware polls the FLAG register's `I1CNT` conversion counter back to back (~10 polls/ms).
  When it changes, it reads `RDI` and reads FLAG again; if a conversion landed in between,
  the pair is redone. So every conversion is sent exactly once, and a skipped one shows up as a
  `gap` > 1 instead of passing silently.
- `t_s` is the MCU time at which the conversion was first seen, so it lags the end of the
  conversion by at most one poll (~100 µs). The MCU clock is the internal HSI (±1%).
- Scaling: I1 = code × 1 µV / R_shunt (20 mA per LSB at 50 µΩ, ±130 mV ≈ ±2.6 kA full scale).
  I2 has a −1 µV LSB, so with both inputs on the same shunt the two read the same sign.

## Sources

Command codes, register layouts and timings come from the public ADBMS2950B data sheet Rev. 0
(Table 2, Table 8, Table 33) and were cross-checked against the open-source Northeastern Electric
Racing `adbms2950` driver (FLAG/STAT/SID bit positions). PEC15/PEC10 are checked bit-for-bit
against the ADI implementation in EV27 Tractive_Battery.

# Contributing to EV26-Software

Guide for getting set up and contributing. The firmware-setup section captures
the toolchain steps that are easy to lose between team generations.

## Repository layout

| Folder | Subsystem | Toolchain |
|---|---|---|
| `High Voltage/Tractive Battery` | BMS firmware | STM32 (G474) — CMake / VS Code |
| `High Voltage/Charging Cart` | Charger UI firmware | STM32 (F042) — CMake / VS Code |
| `Controls` | Vehicle control models | MATLAB / Simulink → TI C2000 (F28379D) |
| `CAN` | CAN message definitions | — |
| `BMS` | Battery management | — |
| `Low Voltage` | LV systems | — |
| `MoTeC` | Data / config | MoTeC tools |
| `Exploration` | Prototypes / scratch | varies |

<!-- Update this table when subsystems are added or moved. -->

## Firmware (STM32) setup

1. Install **STM32CubeMX** (graphical config / code generation).
2. Install **STM32CubeCLT** (compiler, CMake, Ninja, ST-LINK GDB server,
   programmer) — free from st.com (Download as Guest if you have no account).
3. Install **VS Code** with the **STM32 VS Code Extension**, **CMake Tools**,
   and **C/C++** extensions.
4. Open the project folder (e.g. `High Voltage/Tractive Battery`).
5. Run **Set Up STM32Cube project(s)** from the STM32 sidebar (device + GCC).
6. Select the **Debug** configure preset, then **Build** from the status bar.
7. Flash/debug with **F5** (board connected via ST-LINK).

To regenerate HAL / pin config: open the project's `.ioc` in STM32CubeMX, set
**Toolchain/IDE = CMake**, and **Generate Code**. Then re-add any custom module
sources to `CMakeLists.txt` if new folders were introduced.

<!-- Pin the CubeCLT / CubeMX versions the team standardizes on here. -->

## Controls (Simulink / C2000) setup

<!-- Fill in:
- Required MATLAB + Simulink + Embedded Coder version
- How to run codegen for the F28379D
- How to flash / which tool
- Which models are deployed vs simulation-only
-->

## Workflow

1. Branch from `main`: `feature/short-description` (or `fix/...`).
2. Commit focused changes; don't commit build output (`.gitignore` covers it).
3. Open a Pull Request and fill in the template.
4. CI must pass and the subsystem owner (see `CODEOWNERS`) must approve.
5. Merge once green and approved.

## Notes

- Folder names currently contain spaces. If renaming to underscores, use
  `git mv` to preserve history and update CI `paths:` filters and `CODEOWNERS`.
- Keep `CMakePresets.json` tracked; `CMakeUserPresets.json` is per-developer
  and ignored.

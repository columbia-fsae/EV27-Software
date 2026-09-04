# Contributing to EV26-Software

Guide for getting set up and contributing. The firmware-setup section captures
the toolchain steps that are easy to lose between team generations.

## Repository layout

| Folder | Subsystem | Toolchain |
|---|---|---|
| `High_Voltage/Tractive_Battery` | BMS firmware | STM32 (G474) — CMake / VS Code |
| `High_Voltage/Charging_Cart` | Charger UI firmware | STM32 (F042) — CMake / VS Code |
| `Controls` | Vehicle control models | MATLAB / Simulink → TI C2000 (F28379D) |
| `CAN` | CAN message definitions | — |
| `BMS` | Battery management | — |
| `Low Voltage` | LV systems | — |
| `MoTeC` | Data / config | MoTeC tools |
| `Exploration` | Prototypes / scratch | varies |

<!-- Update this table when subsystems are added or moved. -->

## What to install

| Tool | Needed for | Install |
|---|---|---|
| **Python 3.11+** | Linting, `pre-commit`, sim scripts | python.org / your package manager |
| **pip packages** | `ruff`, `pre-commit`, `pytest` | `pip install -r requirements.txt` (repo root) |
| **CMake, Ninja, `arm-none-eabi-gcc`** | Building STM32 firmware from the CLI (matches CI exactly) | see *Firmware (STM32) setup* below |
| **`.clang-format` / `.clang-tidy`** | C/C++ style — already committed at repo root, nothing to install | — |
| **MATLAB + Simulink** | Editing `.slx`/`.mdl` models, running the C2000 setup below | licensed install from MathWorks |

After installing Python:

```sh
pip install -r requirements.txt
pre-commit install   # runs the hooks in .pre-commit-config.yaml on every `git commit`
```

`pre-commit install` only needs to be run once per clone. From then on, `git
commit` runs the same ruff/clang-format/hygiene checks CI does
([lint.yml](.github/workflows/lint.yml),
[.pre-commit-config.yaml](.pre-commit-config.yaml)) — so failures show up
locally before a push, not after. `clang-tidy` is excluded from the default
run (`stages: [manual]`) since it needs a CMake compile database; run it
explicitly after configuring a project (see below):

```sh
pre-commit run --hook-stage manual clang-tidy
```

## Firmware (STM32) setup

### Option A — VS Code (recommended for day-to-day dev/debug)

1. Install **STM32CubeMX** (graphical config / code generation).
2. Install **STM32CubeCLT** (compiler, CMake, Ninja, ST-LINK GDB server,
   programmer) — free from st.com (Download as Guest if you have no account).
3. Install **VS Code** with the **STM32 VS Code Extension**, **CMake Tools**,
   and **C/C++** extensions.
4. Open the project folder (e.g. `High_Voltage/Tractive_Battery`).
5. Run **Set Up STM32Cube project(s)** from the STM32 sidebar (device + GCC).
6. Select the **Debug** configure preset, then **Build** from the status bar.
7. Flash/debug with **F5** (board connected via ST-LINK).

To regenerate HAL / pin config: open the project's `.ioc` in STM32CubeMX, set
**Toolchain/IDE = CMake**, and **Generate Code**. Then re-add any custom module
sources to `CMakeLists.txt` if new folders were introduced.

<!-- Pin the CubeCLT / CubeMX versions the team standardizes on here. -->

### Option B — command line (matches CI exactly)

CI ([firmware-build.yml](.github/workflows/firmware-build.yml)) builds with
plain `cmake`/`ninja`/`arm-none-eabi-gcc`, no IDE required — useful for
reproducing a CI failure locally:

```sh
# Debian/Ubuntu/WSL:
sudo apt-get install cmake ninja-build gcc-arm-none-eabi
# macOS: brew install cmake ninja
#        + ARM GNU Toolchain 14.2.Rel1 from developer.arm.com

cd High_Voltage/Tractive_Battery   # or High_Voltage/Charging_Cart/UI/STM32
cmake --preset Debug
cmake --build --preset Debug
```

Pin to ARM GCC **14.2.Rel1** if you want an exact match with CI
(`arm-none-eabi-gcc --version`); a newer 14.x from your package manager is
usually fine too.

## Controls (Simulink / C2000) setup

<!-- Fill in:
- Required MATLAB + Simulink + Embedded Coder version
- How to run codegen for the F28379D
- How to flash / which tool
- Which models are deployed vs simulation-only
-->

### Git integration for `.slx`/`.mdl` (one-time, per machine)

`.gitattributes` routes `.slx`/`.slxc` conflicts through MATLAB's own 3-way
merge instead of failing outright — but that requires a *local* git config
entry pointing at your MATLAB install, which can't be committed to the repo.
Run once per machine (Windows):

```powershell
.\scripts\setup-matlab-git.ps1
```

It finds your MATLAB install and wires up `git config --global` entries for
`merge.mlAutoMerge`, `mergetool.mlMerge`, and `difftool.mlDiff`. Without this,
a conflicting `.slx` change will still just hard-conflict in git — resolve it
by hand in Simulink (`git mergetool`) rather than losing data by picking
"ours"/"theirs" blindly.

## Workflow

1. Branch from `main`: `feature/short-description` (or `fix/...`).
2. Commit focused changes; don't commit build output (`.gitignore` covers it).
3. Open a Pull Request and fill in the template.
4. CI must pass and the subsystem owner (see `CODEOWNERS`) must approve.
5. Merge once green and approved.

## Notes

- Keep `CMakePresets.json` tracked; `CMakeUserPresets.json` is per-developer
  and ignored.

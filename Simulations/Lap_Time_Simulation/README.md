# Lap Time Simulation

A point-mass lap time simulator for FSAE EV vehicle design studies: given a car
(mass, aero, tires, drivetrain, motor) and a track (a sequence of straight/constant-radius
segments), it computes a feasible speed trace around the lap, the resulting lap time,
power/energy usage, and FSAE competition points.

This is a Python/NumPy/Numba port of a set of MATLAB scripts
(`point_mass_sim_*.m`, `test_graph*.m`, `track_layout.m`) that had accumulated a lot of
copy-pasted duplication across different design studies. The simulation physics and
algorithm are unchanged; the code has been reorganized into a reusable object-oriented
library (`lap_sim/`) with each original study reproduced as a short script in
`experiments/`.

## Layout

- `lap_sim/` — the simulation library.
  - `motors.py`, `vehicle.py`, `regulations.py`, `track.py` — data model (motors, cars,
    competition rules, tracks), including presets matching the original MATLAB constants.
  - `dynamics.py` — the numeric core: force balance, corner speed limits, the
    forward/backward speed-trace evolution, and power/energy accounting. Numba-JIT'd for
    speed.
  - `lap_simulator.py` — `LapSimulator` ties the data model to the dynamics core and
    produces a `LapResult`.
  - `results.py` — `LapResult`/`LapStats`, the per-point simulation output.
  - `scoring.py` — FSAE competition points formula (`CompetitionScorer`).
  - `sweeps.py` — parameter-sweep helpers used by the design-study scripts.
  - `geometry.py` / `plotting.py` — track geometry reconstruction and all matplotlib
    visualizations.
  - `battery.py` — a currently-inert extension point for future battery/cell-level
    modeling (see "Extending" below).
- `experiments/` — one script per original MATLAB study (see table below).
- `tests/` — basic sanity checks on the dynamics core.

| Script | Reproduces | What it does |
|---|---|---|
| `experiments/run_events.py` | `point_mass_sim_final.m` | Runs accel, skidpad, Icahn loop, and endurance; plots and scores them |
| `experiments/gear_ratio_sweep.py` | `point_mass_sim_gear_ratio.m` | Sweeps final drive ratio, plots accel time vs. ratio |
| `experiments/mass_power_grid_sweep.py` | `point_mass_sim_mass_powerlim208.m` + `test_graph.m` | Grid sweep over power limit x mass, heatmaps of score/peak power/avg power |
| `experiments/mass_energy_power_search.py` | `point_mass_sim_mass_endur.m` + `test_graph_energy.m` | For each mass x energy-budget combination, searches for the best power limit; heatmaps + energy-density overlay lines |
| `experiments/track_geometry_demo.py` | `track_layout.m` | Reconstructs and plots track centerlines from segment data |

## Usage

```bash
pip install -r requirements.txt
python experiments/run_events.py
```

Each experiment script is runnable directly and opens matplotlib figures. Grid-sweep
scripts (`mass_power_grid_sweep.py`, `mass_energy_power_search.py`) run many lap
simulations and can take a while on first run (Numba JIT warmup adds a few seconds too).

## Extending

`lap_sim/battery.py` defines a `BatteryModel` interface and a `Car.battery` slot that
isn't yet wired into the physics (today's model uses a flat `Regulations.power_limit`,
same as the original MATLAB). It exists as the intended attachment point for a future
battery/cell-level model (voltage sag, thermal derating, SOC-dependent power limits, etc.)
to eventually influence available power in `dynamics.py`.

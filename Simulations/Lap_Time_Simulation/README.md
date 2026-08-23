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
    forward/backward speed-trace evolution, and power/energy accounting.
  - `lap_simulator.py` — `LapSimulator` ties the data model to the dynamics core and
    produces a `LapResult`.
  - `results.py` — `LapResult`/`LapStats`, the per-point simulation output.
  - `scoring.py` — FSAE competition points formula (`CompetitionScorer`).
  - `sweeps.py` — parameter-sweep helpers used by the design-study scripts.
  - `geometry.py` / `plotting.py` — track geometry reconstruction and all matplotlib
    visualizations.
  - `tire.py` — tire model: simple friction-ellipse (`mu_x`/`mu_y`), or a lookup table
    built from measured/simulated friction-ellipse data (`tire_ellipse_cache.pkl`).
  - `cell.py` / `battery.py` — cell equivalent-circuit model (SOC-dependent 2RC Thevenin
    network, per-cell parameters loaded from `cell_data/*.csv`) and the series/parallel
    pack built from it. `Car.battery` is optional; set it on a `Car` to have SOC-dependent
    available power shape `dynamics.py`'s force balance over the lap (see "Extending"
    below).
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

`Car.battery` is an optional slot (default `None`, meaning "no battery model" -- available
power is limited only by `Regulations.power_limit`, same as the original MATLAB). Set it to
a `lap_sim.battery.Battery(series, parallel, cell_type)` and `dynamics.py` derates available
power by that pack's SOC-dependent current limit over the course of the lap, using the cell
equivalent-circuit model in `cell.py`. To add a new cell, drop a per-SOC fitted-parameter CSV
(columns `SOC, R0, R1, C1, R2, C2, OCV, OCV slope`) into `lap_sim/cell_data/` and register it
in `cell.CELL_OPTIONS`.

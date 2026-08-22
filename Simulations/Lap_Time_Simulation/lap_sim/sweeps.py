"""Parameter-sweep helpers used by the design-study scripts.

`CompetitionEvaluator` replaces the `compete()`/`vary_power_mix()` helpers that were
copy-pasted (with tiny variations) across `point_mass_sim_gear_ratio.m`,
`point_mass_sim_mass_powerlim208.m`, and `point_mass_sim_mass_endur.m`: run the accel,
skidpad, and endurance events for a given car/ruleset and score the result.
"""
from __future__ import annotations

import itertools
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass
from typing import Callable, Optional

import numpy as np
import pandas as pd

from .lap_simulator import LapSimulator
from .regulations import Regulations
from .scoring import CompetitionScorer
from .track import Track
from .vehicle import Car


@dataclass(eq=False)
class CompetitionEvaluator:
    accel_track: Track
    skidpad_track: Track
    endurance_track: Track
    scorer: CompetitionScorer
    dx: float = 0.01
    laps: int = 22

    def evaluate(self, car: Car, regulations: Regulations, include_efficiency: bool = True) -> dict:
        accel = LapSimulator(regulations, self.accel_track, car, dx=self.dx).run()
        # The 75 m timed acceleration run is segments 2-3 (0-indexed); segment 0 is the
        # rollout box before the timing gate, segments 3-4 are the post-finish stopping zone.
        accel_time = accel.split_time(1, 2)

        skidpad = LapSimulator(regulations, self.skidpad_track, car, dx=self.dx).run()
        # The scored lap is segment 3 (0-indexed) - the second circuit of the right circle.
        skidpad_time = skidpad.split_time(2, 3)

        endurance = LapSimulator(regulations, self.endurance_track, car, dx=self.dx).run()
        endurance_time = endurance.lap_time * self.laps
        avg_electric = endurance.stats.avg_electric
        peak_electric = float(np.max(endurance.stats.pelectric))
        energy_wh = avg_electric * endurance_time / 3600.0
        autocross_time = endurance_time * 0.8 / self.laps

        score = self.scorer.score(
            accel_time, skidpad_time, autocross_time, endurance_time,
            energy_wh if include_efficiency else None,
        )
        return {
            "accel_time": accel_time,
            "skidpad_time": skidpad_time,
            "endurance_time": endurance_time,
            "avg_electric_w": avg_electric,
            "peak_electric_w": peak_electric,
            "energy_wh": energy_wh,
            "score": score.total,
            "score_breakdown": score,
        }


@dataclass(eq=False)
class PowerEnergySearch:
    """Reproduces `point_mass_sim_mass_endur.m`'s search: starting from the motor's hard
    power limit, step the regulatory power limit down until both the endurance peak
    electrical power and total energy consumption fall within target bounds.
    """

    evaluator: CompetitionEvaluator
    power_step: float = 3e3
    peak_power_limit: float = 80e3

    def find(self, base_car: Car, base_regulations: Regulations, mass: float, energy_target_wh: float) -> dict:
        car = base_car.replace(mass=mass)
        power = car.drivetrain.motor.hard_power_limit
        peak_power = float("inf")
        energy_wh = float("inf")
        result = None
        while peak_power > self.peak_power_limit or energy_wh > energy_target_wh:
            power -= self.power_step
            regulations = base_regulations.replace(power_limit=power)
            result = self.evaluator.evaluate(car, regulations, include_efficiency=True)
            peak_power = result["peak_electric_w"]
            energy_wh = result["energy_wh"]
        result = {**result, "power_limit": power, "mass": mass, "energy_target_wh": energy_target_wh}
        return result


def grid_sweep(param_grid: dict, evaluate_fn: Callable[..., dict], n_workers: int = 1) -> pd.DataFrame:
    """Evaluate `evaluate_fn(**params)` over every combination in `param_grid`.

    Replaces the `meshgrid` + `arrayfun` + `reshape` pattern used for the power-limit x
    mass sweep in `point_mass_sim_mass_powerlim208.m`, returning a tidy DataFrame (one
    row per combination) instead of parallel 2D arrays.

    `n_workers` > 1 fans the combinations out across a process pool instead of running
    them one at a time. This is only safe/useful because every grid cell is independent
    -- each call builds its own fresh Car/Battery and touches no state shared with any
    other cell -- so cells can run in any order or process with the same result.
    `evaluate_fn` must be a module-level (picklable) callable for this to work; a
    closure defined inside another function can't be sent to a worker process.
    """
    keys = list(param_grid.keys())
    param_dicts = [dict(zip(keys, combo)) for combo in itertools.product(*(param_grid[k] for k in keys))]

    if n_workers == 1:
        results = [evaluate_fn(**params) for params in param_dicts]
    else:
        with ProcessPoolExecutor(max_workers=n_workers) as executor:
            futures = [executor.submit(evaluate_fn, **params) for params in param_dicts]
            results = [f.result() for f in futures]

    rows = [{**params, **result} for params, result in zip(param_dicts, results)]
    return pd.DataFrame(rows)

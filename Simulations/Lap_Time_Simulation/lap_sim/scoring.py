"""FSAE competition points formula (`lap2score` in the original scripts).

The original had two versions floating around: an older one used only in
`point_mass_sim_mass_powerlim208.m` that scores accel/skidpad/autocross/endurance only,
and a newer one (used everywhere else) that adds an efficiency factor on top, computed
from the endurance energy consumption. Both are reproduced here via `CompetitionScorer`:
call `.score(..., energy_wh=None)` for the old behavior, or pass an energy figure for the
new one.
"""
from __future__ import annotations

from dataclasses import dataclass


@dataclass(eq=False)
class ScoreBreakdown:
    accel: float
    skidpad: float
    autocross: float
    endurance: float
    efficiency: float

    @property
    def total(self) -> float:
        return self.accel + self.skidpad + self.autocross + self.endurance + self.efficiency


@dataclass(eq=False)
class CompetitionScorer:
    """FSAE points formula, parameterized by reference times/efficiency benchmarks.

    Defaults are the 2025 Michigan competition benchmarks used in the original scripts.
    """

    accel_min: float = 3.821
    skidpad_min: float = 4.933
    autocross_min: float = 45.734
    endurance_min: float = 1369.936
    efficiency_factor_max: float = 0.848  # UConn 2025
    efficiency_factor_min: float = (62.270 / (1.45 * 62.270)) * (
        (3.275 * 0.65 / 22) / (20.02 * 22 / (100 * 22))
    )  # ~0.483: min time SJSU, min CO2 UConn

    def score(
        self,
        accel_time: float,
        skidpad_time: float,
        autocross_time: float,
        endurance_time: float,
        energy_wh: float | None = None,
    ) -> ScoreBreakdown:
        accel = self._tiered_score(accel_time, self.accel_min, 1.5, 100, 4.5)
        skidpad = self._skidpad_score(skidpad_time)
        autocross = self._tiered_score(autocross_time, self.autocross_min, 1.45, 125, 6.5)
        endurance = self._endurance_score(endurance_time)
        efficiency = 0.0
        if energy_wh is not None:
            efficiency = self._efficiency_score(endurance_time, energy_wh)
        return ScoreBreakdown(accel, skidpad, autocross, endurance, efficiency)

    @staticmethod
    def _tiered_score(time, best, max_ratio, max_points, min_points):
        if time <= 0:
            return 0.0
        if time < best:
            return float(max_points)
        worst = max_ratio * best
        if time < worst:
            return min_points + (max_points - min_points) * ((worst / time) - 1) / ((worst / best) - 1)
        return float(min_points)

    def _skidpad_score(self, time):
        if time <= 0:
            return 0.0
        best, worst = self.skidpad_min, 1.25 * self.skidpad_min
        if time < best:
            return 75.0
        if time < worst:
            return 3.5 + 71.5 * ((worst / time) ** 2 - 1) / ((worst / best) ** 2 - 1)
        return 3.5

    def _endurance_score(self, time):
        if time <= 0:
            return 0.0
        best, worst = self.endurance_min, 1.45 * self.endurance_min
        if time < best:
            return 275.0
        if time < worst:
            return 25 + 250 * ((worst / time) - 1) / ((worst / best) - 1)
        return 25.0

    def _efficiency_score(self, endurance_time, energy_wh):
        best, worst = self.endurance_min, 1.45 * self.endurance_min
        if endurance_time <= 0 or not (best <= endurance_time < worst):
            return 0.0
        laps = 22
        efficiency_factor = (62.270 / (endurance_time / laps)) * (
            (3.275 * 0.65 / laps) / (energy_wh / 1000 * 0.65 / laps)
        )
        score = 100 * (efficiency_factor - self.efficiency_factor_min) / (
            self.efficiency_factor_max - self.efficiency_factor_min
        )
        return score

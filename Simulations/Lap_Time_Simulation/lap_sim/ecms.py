"""Equivalent Consumption Minimization Strategy: an energy-pacing controller that gates
how much of the battery-blind speed trace's implied current the pack is actually allowed
to deliver, so the pack depletes on schedule across all `n_laps` of an endurance run
instead of just always taking whatever `Battery.available_power()` allows.

Per instant, current is chosen to minimize the Hamiltonian
    H(i) = L(i) + lambda_price * delta_soc_used(i)
where `L(i) = 0.5 * k_perf * (i_request - i)^2` penalizes falling short of the
battery-blind trace's implied current `i_request`, and `delta_soc_used(i) = dt/q_pack * i`
is the SOC a pack-level current `i` consumes over `dt` (mirrors `Cell._step`'s SOC row,
scaled from per-cell `q_nom` to pack-level capacity via `parallel`). Minimizing gives

    i* = i_request - lambda_price * dt / (q_pack * k_perf)

`q_pack` is O(10^4-10^5) Coulombs, so `lambda_price` itself would need to reach similar
magnitude before it visibly affects `i*` -- an awkward thing to hand-tune. This class
instead exposes and adapts `lam`, the *backoff current* `lambda_price * dt / (q_pack *
k_perf)` directly (in Amps, dropping the dt-dependence as a deliberate simplification --
dt varies only mildly point to point over a lap): `i* = i_request - lam`. `lam` is then
adapted via PI feedback on tracking error against a reference SOC-vs-distance schedule,
so it grows when the pack is draining faster than the schedule allows (current gets
throttled harder) and shrinks when it's running ahead (more current gets granted).
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable

import numpy as np

from .battery import Battery


@dataclass(eq=False)
class EcmsController:
    """Stateful across an entire `run_multi_lap` call -- construct one instance per car
    per run (mirrors `car.battery`, which is also mutated in place lap over lap) and
    thread it through as `car.ecms`; do not share one instance across multiple runs.

    `kp`/`ki` are in Amps of backoff per unit of SOC tracking error (a 0-1 fraction), so
    e.g. `kp=500` means "back off 500 A instantly for every 100% the pack is behind
    schedule" (5 A per 1% behind). Track- and pack-specific -- validate with
    `command_current`/`update_lambda` directly against representative `i_request`/dt
    values before trusting a tune in a full lap-sim run.
    """
    soc_ref_fn: Callable[[float], float]  # cumulative distance (m) -> reference SOC [0, 1]
    kp: float = 500.0  # proportional gain, Amps of backoff per unit SOC-error
    ki: float = 20.0  # integral gain, Amps of backoff per unit SOC-error-seconds accumulated
    lam_max: float = 400.0  # anti-windup ceiling on the backoff (Amps); keep below typical i_request
    lam: float = 0.0  # current backoff (Amps, >= 0), carried lap-to-lap
    distance_traveled: float = 0.0  # cumulative distance across every lap seen so far
    _integral: float = field(default=0.0, repr=False)

    def command_current(self, battery: Battery, i_request: float, dt: float) -> float:
        """Minimizes H(i) for this step (i* = i_request - lam), then clips to what the
        pack can physically do."""
        if not np.isfinite(dt):
            return 0.0
        i_star = i_request - self.lam
        i_max = battery.available_power() / battery.voltage  # pack-current ceiling, reuses the existing headroom check
        return float(np.clip(i_star, 0.0, i_max))

    def update_lambda(self, soc_actual: float, dx: float, dt: float) -> None:
        """PI feedback on schedule-tracking error -- the actual 'adaptation' in ECMS.
        Also advances `distance_traveled`, so `soc_ref_fn` sees genuine cumulative
        distance across every lap this controller has been stepped through.
        """
        self.distance_traveled += dx
        if not np.isfinite(dt):
            return
        e = self.soc_ref_fn(self.distance_traveled) - soc_actual  # > 0 => behind schedule, used too much already
        self._integral += e * dt
        self.lam = float(np.clip(self.kp * e + self.ki * self._integral, 0.0, self.lam_max))


def linear_soc_schedule(
    total_distance: float, soc_start: float = 1.0, derate: float = 0.0,
) -> Callable[[float], float]:
    """Reference SOC trajectory from `soc_start` to 0 over `total_distance` meters (e.g.
    `n_laps * track.total_length` for endurance).

    `derate=0` (default) is pure linear-in-distance. `derate>0` bows the curve below the
    linear line (`soc_ref = soc_start * (1-x)**(1+derate)`, `x = distance/total_distance`)
    so the schedule expects *faster* depletion than linear as distance accumulates -- a
    cheap way to account for the fact that a pack's usable range per remaining Ah shrinks
    late in a run (internal-resistance losses and voltage sag both grow as SOC drops), so
    treating the last lap's energy as worth exactly as much distance as the first is
    optimistic. Swap in a lap- or section-weighted curve instead if some laps/sections are
    known to be more energy-hungry than others.
    """
    def soc_ref(distance: float) -> float:
        x = min(max(distance / total_distance, 0.0), 1.0)
        return soc_start * (1.0 - x) ** (1.0 + derate)
    return soc_ref

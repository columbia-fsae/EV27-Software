"""Equivalent Consumption Minimization Strategy: an energy- and thermal-pacing controller
that gates how much of the battery-blind speed trace's implied current the pack is
actually allowed to deliver, so the pack depletes on schedule (and the cell stays under
its thermal budget) across all `n_laps` of an endurance run, instead of just always
taking whatever `Battery.available_power()` allows.

Per instant, current is chosen to minimize the Hamiltonian
    H(I) = 0.5*k*(Ir - I)^2 + s1*Voc*I + s2*r0*I^2
where the first term penalizes falling short of the battery-blind trace's implied
current `Ir`, `s1*Voc*I` is the (dimensionless-`s1`) price of the SOC this pack current
consumes, and `s2*r0*I^2` is the (dimensionless-`s2`) price of the heat it generates in
one cell.

Minimizing H over I gives `I* = (k*Ir - s1*Voc) / (k + 2*s2*k_temp*r0)`, 
`k_temp` is a second scale constant with no counterpart in the original
derivation, needed because `s1` and `s2`'s natural leverage over I differ by ~6 orders
of magnitude: `s1`'s lever is `Voc` (10^2-10^3 V), `s2`'s is `r0` (10^-4-10^-2 Ohms). No


`s1`/`s2` are each adapted via PI feedback on tracking error against a reference
SOC-vs-distance / temperature-vs-distance schedule: `s1` grows (more current throttled)
when the pack is draining faster than its SOC schedule allows and shrinks when running
ahead; `s2` grows when the cell is hotter than its temperature schedule allows.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable

import numpy as np

from .battery import Battery


@dataclass(eq=False)
class EcmsController:
    """
    `s1`/`s2` are dimensionless prices: `s1_kp`/`s1_ki` are in
    units of `s1` per unit SOC tracking error (a 0-1 fraction); 
    `s2_kp`/`s2_ki` are in units of `s2` per unit temperature tracking error (deg C).
    """
    soc_ref_fn: Callable[[float], float]  # cumulative distance (m) -> reference SOC [0, 1]
    temp_ref_fn: Callable[[float], float]  # cumulative distance (m) -> reference cell temp (deg C)
    k: float = 100.0  # tracking-cost weight on (Ir-I)^2; same units as r0 (Ohms) for H to be power-consistent throughout
    k_temp: float = 1.0  # decouples s2's leverage from k/r0 -- see module docstring; seed near k/r0
    s1_kp: float = 5.0  # proportional gain, s1 per unit SOC-error
    s1_ki: float = 0.2  # integral gain, s1 per unit SOC-error-seconds accumulated
    s2_kp: float = 0.5  # proportional gain, s2 per unit temperature-error (deg C)
    s2_ki: float = 0.02  # integral gain, s2 per unit temperature-error-seconds accumulated
    s1_max: float = 50.0  # anti-windup ceiling on the SOC-error PI term added to s1_0
    s2_max: float = 50.0  # anti-windup ceiling on the temp-error PI term added to s2_0
    s1: float = 0.0  # current SOC price (dimensionless, >= s1_0), carried lap-to-lap
    s2: float = 0.0  # current temperature price (dimensionless, >= s2_0), carried lap-to-lap
    s1_distance_traveled: float = 0.0  # cumulative distance across every lap seen so far
    s2_distance_traveled: float = 0.0
    s1_integral: float = field(default=0.0, repr=False)
    s2_integral: float = field(default=0.0, repr=False)
    s1_0: float = 1.0  # floor price for s1 (PI term is clipped >= 0, so s1 never drops below this)
    s2_0: float = 1.0  # floor price for s2

    def command_current(self, battery: Battery, i_request: float, v_ocv: float, r0: float, dt: float) -> float:
        """Minimizes H(I) for this step (I* = (k*Ir - s1*Voc)/(k + 2*s2*k_temp*r0)),
        then clips to what the pack can physically do. `v_ocv`/`r0` must already be
        pack-referred 
        """
        if not np.isfinite(dt):
            return 0.0
        i_star = (1 / (self.k + 2 * self.k_temp * r0 * self.s2)) * (self.k * i_request - self.s1 * v_ocv)
        i_max = battery.available_power() / battery.voltage  # pack-current ceiling, reuses the existing headroom check
        return float(np.clip(i_star, 0.0, i_max))

    def update_s1(self, soc_actual: float, dx: float, dt: float) -> None:
        """
        PI feedback on schedule-tracking error 
        """
        self.s1_distance_traveled += dx
        if not np.isfinite(dt):
            return
        e = self.soc_ref_fn(self.s1_distance_traveled) - soc_actual  # > 0 => behind schedule, used too much already
        self.s1_integral += e * dt
        self.s1 = self.s1_0 + float(np.clip(self.s1_kp * e + self.s1_ki * self.s1_integral, 0.0, self.s1_max))

    def update_s2(self, temp_actual: float, dx: float, dt: float) -> None:
        self.s2_distance_traveled += dx
        if not np.isfinite(dt):
            return
        e = temp_actual - self.temp_ref_fn(self.s2_distance_traveled)  # > 0 => hotter than the budget allows at this distance
        self.s2_integral += e * dt
        self.s2 = self.s2_0 + float(np.clip(self.s2_kp * e + self.s2_ki * self.s2_integral, 0.0, self.s2_max))

def linear_soc_schedule(
    total_distance: float, soc_start: float = 1.0, soc_end: float = 0.05, derate: float = 0.0,
) -> Callable[[float], float]:
    """Reference SOC trajectory from `soc_start` to `soc_end` over `total_distance`
    meters (e.g. `n_laps * track.total_length` for endurance).

    `soc_end` defaults to a 0% safety margin, can be higher not 0:

    `derate=0` (default) is pure linear-in-distance. `derate>0` bows the curve below the
    linear line (`x = distance/total_distance`) so the schedule expects *faster*
    depletion than linear as distance accumulates
    """
    def soc_ref(distance: float) -> float:
        x = min(max(distance / total_distance, 0.0), 1.0)
        return soc_end + (soc_start - soc_end) * (1.0 - x) ** (1.0 + derate)
    return soc_ref

def linear_temp_schedule(
        total_distance: float, temp_start: float = 25, temp_end: float = 60, derate: float = 0.0,
) -> Callable[[float], float]:
    
    def temp_ref(distance: float) -> float:
        x = min(max(distance / total_distance, 0.0), 1.0)
        return temp_start + (temp_end - temp_start) * (x ** (1.0 + derate))
    return temp_ref

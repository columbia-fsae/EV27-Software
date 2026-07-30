"""Extension point for future battery / cell-level modeling.

Today's dynamics use a single flat `Regulations.power_limit` for the whole lap, same as
the original MATLAB — no battery model influences the physics yet. `Car.battery` is an
optional, currently-unused slot; a future battery/cell simulation can implement
`BatteryModel` (e.g. tracking state of charge, voltage sag under load, thermal derating)
and have `LapSimulator`/`dynamics.py` consult it to shape the available power over the
course of a lap or a full run, instead of relying purely on the regulatory cap.
"""
from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass


class BatteryModel(ABC):
    """Interface a future battery/cell model would implement."""

    @abstractmethod
    def available_power(self, state_of_charge: float, temperature_c: float) -> float:
        """Max electrical power (W) the pack can deliver at this SOC/temperature."""

    @abstractmethod
    def voltage(self, state_of_charge: float, current: float) -> float:
        """Terminal voltage (V) at this SOC and current draw."""


@dataclass(eq=False)
class StaticVoltageBattery(BatteryModel):
    """Placeholder battery: fixed voltage window, no SOC/thermal/current effects.

    Mirrors the `hv.vmax` / `hv.vnom` fields already present on `Car` in the original
    MATLAB (tracked as data, never used by the physics). Useful as a starting point to
    subclass once real cell data is available.
    """

    vmax: float
    vnom: float
    max_power: float = float("inf")

    def available_power(self, state_of_charge: float, temperature_c: float) -> float:
        return self.max_power

    def voltage(self, state_of_charge: float, current: float) -> float:
        return self.vnom

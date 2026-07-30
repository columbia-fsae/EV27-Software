"""Vehicle data model: tires, aero, drivetrain, HV system, and the car itself.

Presets below (`EV24`, `EV25`, `EV26A`) match the values shared identically across all of
the original MATLAB design-study scripts. Each script's own "EV26B" variant differs
(mass/aero/ratio/HV numbers genuinely change between studies) and is built directly in
its owning experiment script instead of living here.
"""
from __future__ import annotations

from dataclasses import dataclass, field, replace
from typing import TYPE_CHECKING, Optional

import numpy as np

from .motors import EMRAX_208, EMRAX_268, Motor

if TYPE_CHECKING:
    from .battery import BatteryModel


@dataclass(eq=False)
class Tires:
    mux: float
    muy: float
    radius: float
    rolling_resistance: float = 0.0


@dataclass(eq=False)
class Aero:
    cda: float
    cla: float = 0.0


@dataclass(eq=False)
class Drivetrain:
    motor: Motor
    ratio: float
    efficiency: float
    count: int = 1


@dataclass(eq=False)
class HighVoltageSystem:
    vmax: float
    vnom: float


@dataclass(eq=False)
class Car:
    name: str
    mass: float
    cg: np.ndarray  # [x, y, z] in mm, matching the original struct layout
    aero: Aero
    tires: Tires
    drivetrain: Drivetrain
    hv: HighVoltageSystem
    l: Optional[float] = None  # wheelbase (m); required only for weight-transfer physics
    battery: Optional["BatteryModel"] = None  # future extension hook, inert today

    @property
    def cg_height_m(self) -> float:
        """CG height above ground, in meters (cg[2] is stored in mm)."""
        return self.cg[2] / 1000.0

    def replace(self, **changes) -> "Car":
        return replace(self, **changes)


_TIRES_EV24_25 = Tires(mux=1.5, muy=1.5, radius=0.2032, rolling_resistance=0.015)
_HV_EV24_25 = HighVoltageSystem(vmax=302.4, vnom=260)
_CG_EV24_25 = np.array([738.0, 0.0, 255.41])

EV24 = Car(
    name="EV24",
    mass=264 + 68,
    cg=_CG_EV24_25,
    aero=Aero(cda=0.428, cla=0.0),
    tires=_TIRES_EV24_25,
    drivetrain=Drivetrain(motor=EMRAX_208, ratio=3.7, efficiency=0.96, count=1),
    hv=_HV_EV24_25,
    l=None,  # never exercised by any experiment; wheelbase was left unset in the source
)

EV25 = Car(
    name="EV25",
    mass=242.5 + 68,
    cg=_CG_EV24_25,
    aero=Aero(cda=0.428, cla=0.0),
    tires=_TIRES_EV24_25,
    drivetrain=Drivetrain(motor=EMRAX_208, ratio=4.9, efficiency=0.96, count=1),
    hv=_HV_EV24_25,
    l=1.530,
)

EV26A = Car(
    name="EV26A (2x Emrax 268)",
    mass=260 + 68,
    cg=_CG_EV24_25,
    aero=Aero(cda=0.2288, cla=0.51),
    tires=_TIRES_EV24_25,
    drivetrain=Drivetrain(motor=EMRAX_268, ratio=1, efficiency=0.96, count=2),
    hv=HighVoltageSystem(vmax=600, vnom=520),
    l=1.530,
)

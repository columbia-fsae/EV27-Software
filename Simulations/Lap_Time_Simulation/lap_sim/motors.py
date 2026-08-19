"""Motor definitions: speed-torque envelope and efficiency map.

Ported from the `emrax208` / `emrax228` / `emrax268` / `dhx_k40` structs shared across
the original MATLAB scripts.
"""
from __future__ import annotations

from dataclasses import dataclass, field, replace

import numpy as np


@dataclass(eq=False)
class Motor:
    name: str
    max_rpm: float

    # Speed-torque envelope: max continuous torque (Nm) vs. angular speed (rad/s).
    torque_speed_w: np.ndarray
    torque_speed_m: np.ndarray

    # Efficiency map: efficiency (0-1) over a (speed, torque) grid.
    eff_speed: np.ndarray
    eff_torque: np.ndarray
    eff_map: np.ndarray

    # Hard power limit used as a sweep reference point in the design studies (not part
    # of the per-point force balance itself).
    hard_power_limit: float

    def max_torque(self, omega: np.ndarray) -> np.ndarray:
        """Max available torque (Nm) at motor angular speed(s) `omega` (rad/s)."""
        return np.interp(omega, self.torque_speed_w, self.torque_speed_m)

    def max_power(self, omega: np.ndarray) -> np.ndarray:
        """Max available power (W) at motor angular speed(s) `omega` (rad/s)."""
        return self.max_torque(omega)*omega

    def efficiency(self, omega: np.ndarray, torque: np.ndarray) -> np.ndarray:
        """Efficiency (0-1) at motor angular speed(s)/torque(s), clamped to the map."""
        w = np.clip(omega, self.eff_speed[0], self.eff_speed[-1])
        t = np.clip(torque, self.eff_torque[0], self.eff_torque[-1])
        return _nearest_2d(self.eff_speed, self.eff_torque, self.eff_map, w, t)

    def with_power_limit(self, hard_power_limit: float) -> "Motor":
        return replace(self, hard_power_limit=hard_power_limit)


def _nearest_2d(xg: np.ndarray, yg: np.ndarray, z: np.ndarray, x, y):
    """Nearest-neighbor lookup on a regular (xg, yg) grid; z has shape (len(xg), len(yg))."""
    ix = np.abs(xg[:, None] - np.atleast_1d(x)[None, :]).argmin(axis=0)
    iy = np.abs(yg[:, None] - np.atleast_1d(y)[None, :]).argmin(axis=0)
    result = z[ix, iy]
    return result if np.ndim(x) or np.ndim(y) else result.item()


RPM_TO_RAD_S = 0.10472  # 2*pi/60, matches the original scripts' scale factor


def _rpm(*values: float) -> np.ndarray:
    return RPM_TO_RAD_S * np.asarray(values, dtype=float)


EMRAX_208 = Motor(
    name="Emrax 208",
    max_rpm=7000,
    torque_speed_w=_rpm(0, 4000, 4500, 5000, 5500, 6000, 7000),
    torque_speed_m=np.array([140, 135, 132, 128, 122, 115, 0], dtype=float),
    eff_speed=_rpm(500, 1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000),
    eff_torque=np.array([20, 40, 60, 80, 100, 120, 140], dtype=float),
    eff_map=np.array(
        [
            [86, 88, 89, 88, 86, 84, 82],
            [88, 94, 94, 94, 92, 89, 85],
            [89, 94.5, 95.5, 96, 94.5, 91, 85],
            [89, 95.5, 96, 96, 95, 92, 85],
            [89, 95.5, 96, 96, 95, 92, 85],
            [89, 95, 96, 95.75, 94.75, 92, 85],
            [88, 94.5, 96, 95.5, 94.5, 92, 85],
            [86, 94, 95, 95.25, 94.5, 92, 85],
            [80, 87, 94, 94.5, 94.5, 91, 85],
            [80, 80, 86, 94, 94, 90, 86],
        ],
        dtype=float,
    )
    / 100.0,
    hard_power_limit=61.5e3,
)

EMRAX_228 = Motor(
    name="Emrax 228",
    max_rpm=5000,
    torque_speed_w=_rpm(0, 2000, 3000, 4000, 5000, 5001),
    torque_speed_m=np.array([240, 240, 234, 228, 216, 0], dtype=float),
    eff_speed=EMRAX_208.eff_speed * 5500 / 6000,
    eff_torque=EMRAX_208.eff_torque * 240 / 140,
    eff_map=EMRAX_208.eff_map.copy(),
    hard_power_limit=80e3,
)

EMRAX_268 = Motor(
    name="Emrax 268",
    max_rpm=4500,
    torque_speed_w=_rpm(0, 2000, 4500, 4501),
    torque_speed_m=np.array([500, 500, 450, 0], dtype=float),
    eff_speed=EMRAX_208.eff_speed * 4500 / 6000,
    eff_torque=EMRAX_208.eff_torque * 500 / 140,
    eff_map=EMRAX_208.eff_map.copy(),
    hard_power_limit=80e3,
)

DHX_K40 = Motor(
    name="DHX K40",
    max_rpm=6000,
    torque_speed_w=_rpm(0, 6000, 6001),
    torque_speed_m=np.array([80, 80, 0], dtype=float),
    eff_speed=np.array([0, 7000], dtype=float),
    eff_torque=np.array([0, 80], dtype=float),
    eff_map=np.full((2, 2), 0.93),
    hard_power_limit=40e3,
)

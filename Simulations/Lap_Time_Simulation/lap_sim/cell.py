"""Cell-level equivalent-circuit model (2RC Thevenin network).

Per-cell fitted parameters (R0, R1/C1, R2/C2, OCV, OCV slope, each as a function of SOC)
are loaded from a CSV in `cell_data/`, one file per premade cell -- adding a new cell
means dropping a similarly-shaped CSV into `cell_data/` and registering it in
`CELL_OPTIONS` below.
"""
import csv
import os
from dataclasses import dataclass

import numpy as np

_DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cell_data")

# R0_ARRHENIUS_EA is a literature-typical activation energy for Li-ion ohmic/charge-transfer resistance
# (~20-30 kJ/mol); it only models the well-evidenced cold-temperature resistance rise
# (e.g. Batemo's published discharge curves for this cell fan out below room temp) and is
# left flat above the reference temp rather than guessing a high-temp rise with no
# supporting data. THERMAL_DERATE_START_C/MAX_C instead carry the "hot pack loses
# performance" behavior, as a current derate anchored to Ampace's stated -20/80 degC
# stable-operation range (matching Batemo's observed thermal-plateau ceiling).
_GAS_CONSTANT = 8.314  # J/(mol*K)
_R0_TEMP_REF_C = 25.0  # deg C -- reference temp for the per-SOC R0 curves in cell_data/*.csv
_R0_ARRHENIUS_EA = 25000.0  # J/mol
_THERMAL_DERATE_START_C = 60.0  # deg C -- current derate begins
_THERMAL_DERATE_MAX_C = 80.0  # deg C -- current derated to zero


def _load_cell_csv(filename: str):
    """Read a per-SOC fitted-parameter CSV into SOC-indexed numpy arrays.

    Expects columns named SOC, R0, R1, C1, R2, C2, OCV, "OCV slope" (other columns, e.g.
    the tau/rmse diagnostic fields, are ignored). SOC must already be on the same 0-1
    fraction scale as `Cell`'s internal state (self._x[0], 1.0 == full); "OCV slope" is
    d(OCV)/d(SOC) on that same scale. Returns 2-column [SOC, value] arrays for
    r0/rct/cct/rdif/cdif, and a 3-column [SOC, OCV, OCV slope] array for soc_ocv -- all
    sorted ascending by SOC so they're ready for np.interp.
    """
    path = os.path.join(_DATA_DIR, filename)
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))

    soc = np.array([float(row["SOC"])/100.0 for row in rows])
    order = np.argsort(soc)
    soc = soc[order]

    def column(header):
        values = np.array([float(row[header]) for row in rows])[order]
        return np.column_stack([soc, values])

    r0 = column("R0_mohm")/1000.0
    rct = column("R1_mohm")/1000.0
    cct = column("C1_F")
    rdif = column("R2_mohm")/1000.0
    cdif = column("C2_F")
    ocv = np.array([float(row["OCV_V"]) for row in rows])[order]
    ocv_slope = np.array([float(row["OCV slope"]) for row in rows])[order]
    soc_ocv = np.column_stack([soc, ocv, ocv_slope])

    return r0, rct, cct, rdif, cdif, soc_ocv


# Convective heat rejection to ambient air, per cell (Newton's law of cooling: P = h*A*(T -
# T_ambient)). Previously the cell model was purely adiabatic (all I^2*R0 heat stayed in the
# cell forever) -- with the real HPPC data's ~4.5x higher R0 than the old placeholder, that
# made the thermal-pacing loop throttle current far harder than a cell with any real airflow
# actually needs. h=50 W/(m^2*K) is a placeholder for light forced convection (fan/ram-air
# across the pack); A is estimated from a 21700-format cylinder (21mm dia x 70mm), matching
# this cell's mass/capacity ballpark -- TODO: confirm both against the real pack's cooling
# design and the AMPACE JP50 datasheet.
_COOLING_H_W_M2K = 50.0
_AMBIENT_TEMP_C = 30.0  # matches the sim's TEMP_START convention (hot-day pre-warmed ambient)


@dataclass
class cell_type:
    name: str
    min_v: float
    nom_v: float
    max_v: float
    q_nom: float  # Ah
    mass_kg: float
    specific_heat: float
    surface_area_m2: float
    r0: np.ndarray
    rct: np.ndarray
    cct: np.ndarray
    rdif: np.ndarray
    cdif: np.ndarray
    soc_ocv: np.ndarray


def _make_ampace_jp50() -> cell_type:
    r0, rct, cct, rdif, cdif, soc_ocv = _load_cell_csv("hppc_new_cells_full_0819_fitted_parameters_CORRECTED.csv")
    cell_diameter_m, cell_height_m = 0.021, 0.070  # 21700-format estimate -- see module comment above
    surface_area_m2 = np.pi * cell_diameter_m * cell_height_m + 2 * np.pi * (cell_diameter_m / 2) ** 2
    return cell_type(
        name="ampace_jp50",
        min_v=2.5,
        nom_v=3.6,
        max_v=4.2,
        q_nom=5.0,  # Ah -- TODO: confirm against the AMPACE JP50 datasheet
        mass_kg=0.072,  # kg -- TODO: confirm against the AMPACE JP50 datasheet
        specific_heat=1000.0,
        surface_area_m2=surface_area_m2,
        r0=r0, rct=rct, cct=cct, rdif=rdif, cdif=cdif, soc_ocv=soc_ocv,
    )


_AMPACE_JP50 = _make_ampace_jp50()
CELL_OPTIONS = [_AMPACE_JP50]


class Cell:
    """2RC Thevenin-network cell model, parameterized by SOC-indexed lookup tables."""

    def __init__(self, cell_type: str):
        for cell in CELL_OPTIONS:
            if cell.name == cell_type:
                self._r_0 = cell.r0
                self._r_ct = cell.rct
                self._c_ct = cell.cct
                self._r_dif = cell.rdif
                self._c_dif = cell.cdif
                self._soc_ocv = cell.soc_ocv
                self._min_v = cell.min_v
                self._max_v = cell.max_v
                self._nom_v = cell.nom_v
                self._q_nom = cell.q_nom * 3600  # Ah -> Coulombs (As)
                self._m = cell.mass_kg
                self._c_p = cell.specific_heat
                self._surface_area = cell.surface_area_m2
                break
        else:
            raise ValueError(
                f"Unknown cell_type {cell_type!r}; options are "
                f"{[c.name for c in CELL_OPTIONS]}"
            )

        self._x = np.array([1.0, 0.0, 0.0])  # [SOC, v_ct, v_dif]
        self._T = 30.0

    def _r0_temp_factor(self):
        """Arrhenius correction for cold-temperature R0 rise; flat (1.0) at/above 25 degC."""
        if self._T >= _R0_TEMP_REF_C:
            return 1.0
        t_kelvin = self._T + 273.15
        t_ref_kelvin = _R0_TEMP_REF_C + 273.15
        return np.exp(_R0_ARRHENIUS_EA / _GAS_CONSTANT * (1.0 / t_kelvin - 1.0 / t_ref_kelvin))

    def _thermal_derate_factor(self):
        """Fraction of current still allowed, ramping linearly to 0 as the cell heats
        from _THERMAL_DERATE_START_C toward the _THERMAL_DERATE_MAX_C safety ceiling."""
        if self._T <= _THERMAL_DERATE_START_C:
            return 1.0
        if self._T >= _THERMAL_DERATE_MAX_C:
            return 0.0
        return ((_THERMAL_DERATE_MAX_C - self._T) /
                (_THERMAL_DERATE_MAX_C - _THERMAL_DERATE_START_C))

    @property
    def r_0(self):
        base = np.interp(self._x[0], self._r_0[:, 0], self._r_0[:, 1])
        return base * self._r0_temp_factor()

    @property
    def r_ct(self):
        return np.interp(self._x[0], self._r_ct[:, 0], self._r_ct[:, 1])

    @property
    def c_ct(self):
        return np.interp(self._x[0], self._c_ct[:, 0], self._c_ct[:, 1])

    @property
    def r_dif(self):
        return np.interp(self._x[0], self._r_dif[:, 0], self._r_dif[:, 1])

    @property
    def c_dif(self):
        return np.interp(self._x[0], self._c_dif[:, 0], self._c_dif[:, 1])

    @property
    def ocv(self):
        return np.interp(self._x[0], self._soc_ocv[:, 0], self._soc_ocv[:, 1])

    @property
    def docv_soc(self):
        return np.interp(self._x[0], self._soc_ocv[:, 0], self._soc_ocv[:, 2])

    def cell_curr_limits(self, i: float, dt: float):
        r0 = self.r_0
        r_ct = self.r_ct
        c_ct = self.c_ct
        r_dif = self.r_dif
        c_dif = self.c_dif

        # RC-branch voltages decay from their *current* level (self._x[1]/[2]), not from
        # an implicit 1 -- otherwise this falsely reports ~no headroom at small dt, since
        # exp(-dt/tau) sits near 1 for any dt << tau regardless of how relaxed the cell is.
        v_ct_prev = self._x[1]
        v_dif_prev = self._x[2]

        i_dis = ((self.ocv - v_ct_prev * np.exp(-dt/(r_ct * c_ct)) - v_dif_prev * np.exp(-dt/(r_dif * c_dif)) - self._min_v) /
                 (dt/(self._q_nom / 3600 * 0.9) * self.docv_soc + r_ct * (1 - np.exp(-dt / (r_ct * c_ct))) +
                  r_dif * (1 - np.exp(-dt / (r_dif * c_dif))) + r0))

        return i_dis * self._thermal_derate_factor()

    def _step(self, i: float, dt: float):
        r0 = self.r_0
        r_ct = self.r_ct
        c_ct = self.c_ct
        r_dif = self.r_dif
        c_dif = self.c_dif

        v_ct_prev = self._x[1]
        v_dif_prev = self._x[2]

        a = np.array([
            [1.0, 0.0, 0.0],
            [0.0, np.exp(-dt / (r_ct * c_ct)), 0.0],
            [0.0, 0.0, np.exp(-dt / (r_dif * c_dif))],
        ])
        b = np.array([
            -dt / self._q_nom,
            -r_ct * (1 - np.exp(-dt / (r_ct * c_ct))),
            -r_dif * (1 - np.exp(-dt / (r_dif * c_dif))),
        ])
        self._x = a @ self._x + b * i
        self._x[0] = np.clip(self._x[0], 0.0, 1.0)

        terminal_v = self.ocv - self._x[1] - self._x[2] - r0*i

        p_dis = i**2 * r0 + (self._x[1] - v_ct_prev)**2/r_ct + (self._x[2] - v_dif_prev)**2/r_dif
        p_cooling = _COOLING_H_W_M2K * self._surface_area * (self._T - _AMBIENT_TEMP_C)

        self._T = self._T + (p_dis - p_cooling) / (self._m * self._c_p) * dt

        return self._x[0], terminal_v, self.ocv, self._T

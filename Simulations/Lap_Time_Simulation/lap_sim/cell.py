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

    soc = np.array([float(row["SOC"]) for row in rows])
    order = np.argsort(soc)
    soc = soc[order]

    def column(header):
        values = np.array([float(row[header]) for row in rows])[order]
        return np.column_stack([soc, values])

    r0 = column("R0")
    rct = column("R1")
    cct = column("C1")
    rdif = column("R2")
    cdif = column("C2")
    ocv = np.array([float(row["OCV"]) for row in rows])[order]
    ocv_slope = np.array([float(row["OCV slope"]) for row in rows])[order]
    soc_ocv = np.column_stack([soc, ocv, ocv_slope])

    return r0, rct, cct, rdif, cdif, soc_ocv


@dataclass
class cell_type:
    name: str
    min_v: float
    nom_v: float
    max_v: float
    q_nom: float  # Ah
    mass_kg: float
    specific_heat: float
    r0: np.ndarray
    rct: np.ndarray
    cct: np.ndarray
    rdif: np.ndarray
    cdif: np.ndarray
    soc_ocv: np.ndarray


def _make_ampace_jp50() -> cell_type:
    r0, rct, cct, rdif, cdif, soc_ocv = _load_cell_csv("ampace_jp50.csv")
    return cell_type(
        name="ampace_jp50",
        min_v=2.5,
        nom_v=3.6,
        max_v=4.2,
        q_nom=5.0,  # Ah -- TODO: confirm against the AMPACE JP50 datasheet
        mass_kg=0.072,  # kg -- TODO: confirm against the AMPACE JP50 datasheet
        specific_heat=1000.0,
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
                break
        else:
            raise ValueError(
                f"Unknown cell_type {cell_type!r}; options are "
                f"{[c.name for c in CELL_OPTIONS]}"
            )

        self._x = np.array([1.0, 0.0, 0.0])  # [SOC, v_ct, v_dif]
        self._T = 25.0

    @property
    def r_0(self):
        return np.interp(self._x[0], self._r_0[:, 0], self._r_0[:, 1])

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
                 (dt/(self._q_nom / 3600 * 0.5) * self.docv_soc + r_ct * (1 - np.exp(-dt / (r_ct * c_ct))) +
                  r_dif * (1 - np.exp(-dt / (r_dif * c_dif))) + r0))

        return i_dis

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

        self._T = self._T + p_dis / (self._m * self._c_p) * dt

        return self._x[0], terminal_v, self.ocv, self._T

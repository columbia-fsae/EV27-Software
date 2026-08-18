"""Tire model: either a simple friction-ellipse (mu_x/mu_y) or a lookup table built from
measured/simulated friction-ellipse data, cached to a pickle for fast repeated loads.
"""
import os
import pickle

import numpy as np

TIRE_SIMPLE = "simple"
TIRE_LOOKUP = "lookup"
GRAVITY = 9.81 # m/s^2


def build_tire_ellipse_dict(input_path, lookup_path):
    """Parse raw friction-ellipse CSVs into the `{fz_value: {"fx": [...], "fy": [...]}}`
    shape consumed by `Tire.build_cache`.

    Not yet implemented -- `tire_ellipse_cache.pkl` is currently checked in pre-built.
    Implement this once the raw ellipse CSV format is finalized.
    """
    raise NotImplementedError(
        "build_tire_ellipse_dict is not implemented yet; tire_ellipse_cache.pkl is "
        "currently checked in pre-built, so Tire(type=TIRE_LOOKUP) doesn't need this."
    )


class Tire:
    DEFAULT_CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 "tire_ellipse_cache.pkl")

    def __init__(self,
                 type=TIRE_SIMPLE,
                 mu_x: float = None,
                 mu_y: float = None,
                 radius: float = 0.203,
                 rolling_resistance: float = 0.0,
                 cache_path: str = None):

        self._type = type
        self._mu_x = mu_x
        self._mu_y = mu_y
        self._radius = radius
        self._rolling_resistance = rolling_resistance

        if type == TIRE_LOOKUP:
            if cache_path is None:
                cache_path = self.DEFAULT_CACHE
            with open(cache_path, "rb") as f:
                cached = pickle.load(f)
            self._ellipse_table = cached["data"]
            self._fz_values = np.asarray(cached["fz_values"], dtype=float)

    @classmethod
    def build_cache(cls, input_path, lookup_path, cache_path=None):
        """One-time step: parse the CSVs and write the permanent pickle file.

        Run this once (or whenever the CSVs change). After that, create
        objects with Tire(type=TIRE_LOOKUP) and they read straight from the pickle.
        """
        if cache_path is None:
            cache_path = cls.DEFAULT_CACHE
        data, fz_values = build_tire_ellipse_dict(input_path, lookup_path)
        with open(cache_path, "wb") as f:
            pickle.dump({"data": data, "fz_values": fz_values}, f,
                        protocol=pickle.HIGHEST_PROTOCOL)
        return cache_path

    @property
    def mu_x(self):
        return self._mu_x

    @mu_x.setter
    def mu_x(self, new_mu_x: float):
        self._mu_x = new_mu_x

    @property
    def mu_y(self):
        return self._mu_y

    @mu_y.setter
    def mu_y(self, new_mu_y: float):
        self._mu_y = new_mu_y

    @property
    def radius(self):
        return self._radius

    @radius.setter
    def radius(self, new_radius: float):
        self._radius = new_radius

    @property
    def rolling_resistance(self):
        return self._rolling_resistance

    @rolling_resistance.setter
    def rolling_resistance(self, new_rolling_resistance: float):
        self._rolling_resistance = new_rolling_resistance

    def get_fx(self, fz: float, fy: float, mass: float) -> float:

        if self._type == TIRE_SIMPLE:
            lateral_fraction = min((fy / (fz * self._mu_y)) ** 2, 1.0)
            effective_mux = np.sqrt(1.0 - lateral_fraction) * self._mu_x
            return effective_mux*fz
        else:
            fz_value = self._fz_values[(np.abs(self._fz_values - fz)).argmin()]
            curr_ellipse_table = self._ellipse_table[fz_value]
            fy_arr = np.asarray(curr_ellipse_table["fy"])
            fx_arr = np.asarray(curr_ellipse_table["fx"])
            idx = (np.abs(fy_arr - fy)).argmin()
            fx_out = mass * GRAVITY * np.interp(fy/mass/GRAVITY, fy_arr, fx_arr)
            return abs(np.round(fx_out, decimals=3))

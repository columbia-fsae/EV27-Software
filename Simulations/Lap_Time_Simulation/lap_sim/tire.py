"""Tire model: either a simple friction-ellipse (mu_x/mu_y) or a lookup table built from
measured/simulated friction-ellipse data, cached to a pickle for fast repeated loads.
"""
import os
import pickle

import numpy as np

TIRE_SIMPLE = "simple"
TIRE_LOOKUP = "lookup"


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
    # The cached ellipse tables store fx/fy in kN; fz (and get_fx's fy argument/return)
    # are in N throughout the rest of the codebase (see dynamics.py's force_balance).
    _CACHE_KN_TO_N = 1000.0
    # The cache is per-tire data (confirmed with the team), but get_fx's contract -- to
    # match TIRE_SIMPLE, whose mu_x/mu_y * fz is explicitly whole-car -- takes whole-car
    # fz/fy and returns whole-car fx. Divide the query down to one tire's share and scale
    # the result back up rather than pushing that conversion onto every caller.
    _NUM_TIRES = 4

    def __init__(self,
                 type=TIRE_SIMPLE,
                 mu_x: float = None,
                 mu_y: float = None,
                 radius: float = 0.203,
                 rolling_resistance: float = 0.0,
                 cache_path: str = None,
                 envelope_bins: int = 80):

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
            raw_fz_values = cached["fz_values"]
            self._fz_values = np.asarray(raw_fz_values, dtype=float)
            # The raw per-fz tables are a flattened slip-angle x slip-ratio grid tracing
            # the whole combined-slip boundary (fy loops up and down, not a function of
            # fx) -- not usable with np.interp as-is. Reduce each one, once, to a proper
            # envelope: max available |fx| per |fy| bin, sorted ascending in |fy|.
            self._envelope = {
                float(fz_value): self._build_envelope(
                    np.asarray(cached["data"][raw_key]["fy"], dtype=float),
                    np.asarray(cached["data"][raw_key]["fx"], dtype=float),
                    envelope_bins,
                )
                for raw_key, fz_value in zip(raw_fz_values, self._fz_values)
            }

    @staticmethod
    def _build_envelope(fy_raw, fx_raw, n_bins):
        """Collapse raw combined-slip samples (fy_raw, fx_raw) -- looping, not a
        function of fy -- into a well-posed envelope curve safe for np.interp:
        the max achievable |fx| per bin of |fy|, sorted ascending in |fy|.

        Both branches (drive/brake) collapse into one magnitude curve, matching
        how the caller already treats `get_fx`'s result as a signed-agnostic
        longitudinal force limit (mirroring the TIRE_SIMPLE friction-ellipse model).
        """
        fy_abs = np.abs(fy_raw)
        fx_abs = np.abs(fx_raw)
        max_fy = fy_abs.max()
        if max_fy <= 0.0:
            return np.array([0.0]), np.array([fx_abs.max() if fx_abs.size else 0.0])

        edges = np.linspace(0.0, max_fy, n_bins + 1)
        bin_idx = np.clip(np.digitize(fy_abs, edges) - 1, 0, n_bins - 1)
        bin_fx = np.zeros(n_bins)
        np.maximum.at(bin_fx, bin_idx, fx_abs)
        has_data = np.zeros(n_bins, dtype=bool)
        has_data[bin_idx] = True
        bin_centers = 0.5 * (edges[:-1] + edges[1:])

        fy_env = bin_centers[has_data]
        fx_env = bin_fx[has_data]
        if fy_env[0] > 0.0:
            # At zero lateral demand the tire's full grip budget goes to fx, so the
            # true peak (the dataset's global max |fx|) anchors the fy=0 end of the
            # curve even if no raw sample landed in the first bin.
            fy_env = np.concatenate(([0.0], fy_env))
            fx_env = np.concatenate(([fx_abs.max()], fx_env))
        # bin_centers sit strictly inside their bins, so fy_env's last point is always
        # short of the ellipse's true boundary (max_fy) -- anchoring that boundary here
        # at fx=0 (true by definition: zero longitudinal capacity left at peak lateral
        # force) makes get_fx taper smoothly to zero. Without this, get_fx's `right=0.0`
        # clamp left a hard cliff between the last bin's (nonzero) fx and the boundary,
        # which stalled corner_speed_limit's equilibrium search for any curvature whose
        # steady-state speed landed on that cliff -- it straddled the discontinuity and
        # never converged within tolerance, burning the full max_steps budget every time.
        if fy_env[-1] < max_fy:
            fy_env = np.concatenate((fy_env, [max_fy]))
            fx_env = np.concatenate((fx_env, [0.0]))
        return fy_env, fx_env

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

    def get_fx(self, fz: float, fy: float) -> float:

        if self._type == TIRE_SIMPLE:
            lateral_fraction = min((fy / (fz * self._mu_y)) ** 2, 1.0)
            effective_mux = np.sqrt(1.0 - lateral_fraction) * self._mu_x
            return effective_mux*fz
        else:
            # fz/fy in: whole-car -> per-tire, dividing evenly across all four contact
            # patches (no per-corner load transfer split -- consistent with fz itself
            # already being a lumped GRAVITY*mass+downforce with no left/right or
            # front/rear breakdown).
            fz_tire = fz / self._NUM_TIRES
            fy_tire = fy / self._NUM_TIRES
            # Snap to the nearest pre-built fz bucket -- its envelope is already
            # Fx(Fy) *for that specific fz*, so no further fz-normalization is needed;
            # just convert the query/result between the cache's kN and the rest of the
            # codebase's N.
            fz_value = float(self._fz_values[(np.abs(self._fz_values - fz_tire)).argmin()])
            fy_env, fx_env = self._envelope[fz_value]
            fy_kn = abs(fy_tire) / self._CACHE_KN_TO_N
            # Demanding more lateral force than the table's own ellipse ever achieves
            # is physically impossible -- treat it as zero remaining longitudinal
            # capacity (matching TIRE_SIMPLE's lateral_fraction clamp) rather than
            # letting np.interp silently clamp to the (nonzero) last sampled point.
            fx_kn = np.interp(fy_kn, fy_env, fx_env, right=0.0)
            fx_tire = fx_kn * self._CACHE_KN_TO_N
            # fx out: per-tire -> whole-car, summing all four (equal-share) contributions.
            return abs(np.round(fx_tire * self._NUM_TIRES, decimals=3))

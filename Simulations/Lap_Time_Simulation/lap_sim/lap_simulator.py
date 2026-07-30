"""Adapter between the OOP data model (Car/Track/Regulations) and the numeric core."""
from __future__ import annotations

import numpy as np

from . import dynamics
from .regulations import Regulations
from .results import LapResult, LapStats
from .track import Track
from .vehicle import Car


class LapSimulator:
    """Runs one point-mass lap simulation for a given car/track/ruleset.

    `dx` is the (approximate) simulation step in meters; it is adjusted slightly so the
    track length divides evenly into a whole number of steps.
    """

    def __init__(self, regulations: Regulations, track: Track, car: Car, dx: float = 0.01):
        if car.l is None:
            raise ValueError(
                f"{car.name}: wheelbase (car.l) is required to run a lap simulation "
                "(used by the weight-transfer traction limit)."
            )
        self.regulations = regulations
        self.track = track
        self.car = car
        self.dx_target = dx

    def run(self) -> LapResult:
        track, car, regs = self.track, self.car, self.regulations
        drivetrain = car.drivetrain
        motor = drivetrain.motor

        total_length = track.total_length
        n_points = int(np.ceil(total_length / self.dx_target))
        dx = total_length / n_points
        x = np.arange(n_points) * dx

        seg_idx_per_point = track.segment_index(x)
        curvature_by_point = track.curvature[seg_idx_per_point]

        common_args = (
            car.mass, car.cg_height_m, car.l, car.aero.cda, car.aero.cla,
            track.air_density, car.tires.mux, car.tires.muy, regs.power_limit,
            drivetrain.ratio, float(drivetrain.count), drivetrain.efficiency,
            car.tires.radius, motor.torque_speed_w, motor.torque_speed_m,
        )

        segment_corner_limit = np.array(
            [dynamics.corner_speed_limit(k, *common_args) for k in track.curvature]
        )
        segment_gross_limit = np.minimum(segment_corner_limit, track.limits)
        point_limit = segment_gross_limit[seg_idx_per_point]

        vv = dynamics.trace_speed_profile(point_limit, curvature_by_point, dx, *common_args)

        stats_dict = dynamics.power_and_energy(
            vv, dx, curvature_by_point, car.mass, car.aero.cda, track.air_density,
            drivetrain.ratio, drivetrain.efficiency, car.tires.radius, motor,
        )
        stats = LapStats.from_dynamics_output(stats_dict)

        splits = _segment_finish_times(seg_idx_per_point, stats.tt, track.num_segments)

        return LapResult(
            track=track, car=car, regulations=regs, dx=dx, x=x, vv=vv,
            stats=stats, splits=splits,
        )


def _segment_finish_times(seg_idx_per_point: np.ndarray, tt: np.ndarray, num_segments: int) -> np.ndarray:
    """Cumulative lap time at the last simulation point falling in each segment.

    A segment shorter than the simulation step can end up with no points of its own; it
    is left at 0, matching the original script's behavior.
    """
    splits = np.zeros(num_segments)
    segments = np.arange(num_segments)
    boundaries = np.clip(
        np.searchsorted(seg_idx_per_point, segments, side="right") - 1,
        0, len(seg_idx_per_point) - 1,
    )
    hit = seg_idx_per_point[boundaries] == segments
    splits[hit] = tt[boundaries[hit]]
    return splits

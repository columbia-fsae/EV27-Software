"""Sanity checks on the point-mass dynamics core.

Run directly with `python tests/test_dynamics.py`, or with pytest if available.
"""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lap_sim import ACCEL_EVENT, EV25, FSAE_EV, LapSimulator
from lap_sim.dynamics import GRAVITY, corner_speed_limit
from lap_sim.tire import Tire


def _flat_args(mass=300.0, cg_height=0.25, wheelbase=1.53, cda=0.0, cla=0.0,
                air_density=1.293, mux=1.5, muy=1.5, power_limit=1e9, ratio=1.0,
                count=1.0, efficiency=1.0, tire_radius=0.2032):
    motor_w = np.array([0.0, 10000.0])
    motor_m = np.array([1e6, 1e6])  # effectively unlimited torque
    tire = Tire(mu_x=mux, mu_y=muy, radius=tire_radius)
    # battery=None: these are dynamics-core sanity checks, not battery ones, so leave the
    # car's electrical draw unbounded, same as the effectively-infinite power_limit above.
    return (mass, cg_height, wheelbase, cda, cla, air_density, tire,
            power_limit, ratio, count, efficiency, motor_w, motor_m, None)


def test_straight_line_corner_limit_is_large_and_finite():
    # Needs nonzero drag, or there is genuinely no terminal top speed to converge to.
    v_top = corner_speed_limit(0.0, *_flat_args(cda=0.9))
    assert 0.0 < v_top < 1000.0, v_top


def test_circular_corner_limit_matches_closed_form_without_aero():
    muy = 1.5
    k = 1 / 9.0  # 9 m radius corner
    args = _flat_args(cda=0.0, cla=0.0, muy=muy)
    v_limit = corner_speed_limit(k, *args)
    expected = np.sqrt(GRAVITY * muy / abs(k))
    assert abs(v_limit - expected) / expected < 1e-2, (v_limit, expected)


def test_accel_event_speed_trace_is_plausible():
    result = LapSimulator(FSAE_EV, ACCEL_EVENT, EV25, dx=0.05).run()
    assert np.all(result.vv > 0)
    assert result.vv.max() < 100.0  # m/s, sanity ceiling well above any FSAE car
    assert result.lap_time > 0

    # The accel run should end with a braking zone: the last point's speed should be
    # well below the peak, since the final segment is capped at 0.1 m/s.
    assert result.vv[-1] < result.vv.max()


if __name__ == "__main__":
    tests = [v for k, v in list(globals().items()) if k.startswith("test_")]
    for test in tests:
        test()
        print(f"OK: {test.__name__}")
    print(f"{len(tests)} tests passed")

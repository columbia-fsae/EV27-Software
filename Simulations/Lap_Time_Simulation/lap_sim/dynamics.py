"""The point-mass lap simulation core.

The algorithm, in three stages:

1. **Corner speed limits** — for every distinct track curvature, time-march a point mass
   at that curvature until its longitudinal acceleration settles to ~0; that steady-state
   speed is the fastest the car can sustain through a corner of that radius (limited by
   grip, downforce, and drag).
2. **Speed-trace evolution** — starting from those per-segment limits, walk around the
   closed loop once. Wherever the limit rises from one point to the next, forward-march a
   traction/power/motor-limited acceleration profile until it rejoins the limit curve;
   wherever it falls, back-march a braking profile the same way. Combining every such
   profile with the base limit curve via an elementwise minimum yields a single feasible
   speed trace for the whole lap.
3. **Power/energy accounting** — from the finished speed trace, derive per-point
   traction/brake/drag power, motor torque and efficiency (via the efficiency map), and
   electrical power, plus lap-average figures.

The hot loop is stage 2 (`trace_speed_profile`/`_march`), which is Numba-JIT-compiled.
Everything here operates on plain NumPy arrays/scalars rather than the `Car`/`Track`
objects directly — `lap_simulator.py` is the adapter that unpacks those objects into the
arguments these functions need.
"""
from __future__ import annotations

import numpy as np
from numba import njit

GRAVITY = 9.806  # m/s^2, matches the original scripts' constant


@njit(cache=True)
def force_balance(
    v, k, mass, cg_height, wheelbase, cda, cla, air_density, mux, muy,
    power_limit, ratio, count, efficiency, tire_radius, motor_w, motor_m,
):
    """Longitudinal brake/accel limits (m/s^2) at speed `v` and curvature `k`.

    Combines a friction-circle-derated grip limit with drag, then caps the achievable
    acceleration by whichever of (electrical power, weight-transfer-limited traction,
    motor torque curve) is most restrictive.
    """
    downforce = 0.5 * air_density * cla * v * v
    fz = GRAVITY * mass + downforce
    fy = v * v * k * mass

    lateral_fraction = min((fy / (fz * muy)) ** 2, 1.0)
    effective_mux = np.sqrt(1.0 - lateral_fraction) * mux
    potential_fx = effective_mux * fz

    drag = 0.5 * air_density * cda * v * v
    brake = (-potential_fx - drag) / mass

    power_limited_accel = power_limit / mass / v if v > 0.0 else np.inf

    weight_transfer = (cg_height / wheelbase) * potential_fx / mass / GRAVITY
    traction_limited_accel = 0.5 * potential_fx / mass / (1.0 - weight_transfer)

    omega = v / tire_radius * ratio
    max_torque = np.interp(omega, motor_w, motor_m)
    motor_limited_accel = max_torque * count * ratio * efficiency / tire_radius / mass

    accel_limit = min(power_limited_accel, traction_limited_accel, motor_limited_accel)
    accel = accel_limit - drag / mass
    return brake, accel


@njit(cache=True)
def corner_speed_limit(
    k, mass, cg_height, wheelbase, cda, cla, air_density, mux, muy,
    power_limit, ratio, count, efficiency, tire_radius, motor_w, motor_m,
    time_step=0.001, accel_tolerance=0.001 * GRAVITY,
):
    """Steady-state cornering speed (m/s) for curvature `k`, found by time-marching."""
    v = 0.0
    a = accel_tolerance + 1.0  # ensure at least one iteration
    while abs(a) > accel_tolerance:
        _, a = force_balance(
            v, k, mass, cg_height, wheelbase, cda, cla, air_density, mux, muy,
            power_limit, ratio, count, efficiency, tire_radius, motor_w, motor_m,
        )
        v = v + a * time_step
    return v


@njit(cache=True)
def _march(
    direction, start_idx, v0, dx, limits_snapshot, curvature_by_point,
    mass, cg_height, wheelbase, cda, cla, air_density, mux, muy,
    power_limit, ratio, count, efficiency, tire_radius, motor_w, motor_m,
):
    """Forward (accel) or backward (brake) march from `start_idx`, until the profile
    rejoins `limits_snapshot`. Returns a full-length array equal to `limits_snapshot`
    except along the visited stretch.
    """
    n = limits_snapshot.shape[0]
    profile = limits_snapshot.copy()
    idx = start_idx
    v = v0
    step = dx if direction > 0 else -dx
    while v <= limits_snapshot[idx]:
        k = curvature_by_point[idx]
        brake, accel = force_balance(
            v, k, mass, cg_height, wheelbase, cda, cla, air_density, mux, muy,
            power_limit, ratio, count, efficiency, tire_radius, motor_w, motor_m,
        )
        a = accel if direction > 0 else brake
        v = v + a * step / v
        profile[idx] = v
        idx = (idx + direction) % n
    return profile


@njit(cache=True)
def trace_speed_profile(
    point_limit, curvature_by_point, dx,
    mass, cg_height, wheelbase, cda, cla, air_density, mux, muy,
    power_limit, ratio, count, efficiency, tire_radius, motor_w, motor_m,
):
    """The feasible speed trace around one lap.

    `point_limit` is the per-point speed ceiling before considering accel/braking
    capability (corner grip limits and any externally-imposed segment limits).
    Wherever it steps up or down between consecutive points, an accel/brake profile is
    marched out from that transition and merged in via an elementwise minimum — the
    classic point-mass lap-sim technique.
    """
    n = point_limit.shape[0]
    vv = point_limit.copy()
    for i in range(n):
        p = (i - 1) % n
        if point_limit[p] < point_limit[i]:
            profile = _march(
                1, i, point_limit[p], dx, vv, curvature_by_point,
                mass, cg_height, wheelbase, cda, cla, air_density, mux, muy,
                power_limit, ratio, count, efficiency, tire_radius, motor_w, motor_m,
            )
            vv = np.minimum(vv, profile)
        elif point_limit[p] > point_limit[i]:
            profile = _march(
                -1, p, point_limit[i], dx, vv, curvature_by_point,
                mass, cg_height, wheelbase, cda, cla, air_density, mux, muy,
                power_limit, ratio, count, efficiency, tire_radius, motor_w, motor_m,
            )
            vv = np.minimum(vv, profile)
    return vv


def power_and_energy(vv, dx, curvature_by_point, mass, cda, air_density, ratio,
                      efficiency, tire_radius, motor):
    """Per-point power/energy accounting for a finished speed trace `vv`.

    Returns a dict of per-point arrays plus lap-average figures. Vectorized (not
    JIT-compiled) since it is a single pass with no sequential dependency.
    """
    n = vv.shape[0]
    dt = dx / vv
    tt = np.cumsum(dt)

    ax = (np.roll(vv, -1) - vv) / dt
    ay = vv**2 * curvature_by_point

    f = mass * ax
    drag = 0.5 * cda * air_density * vv**2
    pkinetic = -f * vv
    pdrag = -drag * vv
    ft = f + drag
    traction_mask = ft > 0

    ptraction = np.where(traction_mask, ft * vv, 0.0)
    pmotor = np.where(traction_mask, ptraction / efficiency, 0.0)
    tmotor = np.where(traction_mask, ft * tire_radius / ratio / efficiency, 0.0)
    wmotor = vv / tire_radius * ratio
    numotor = motor.efficiency(wmotor, tmotor)
    pelectric = np.where(traction_mask, pmotor / numotor, 0.0)
    pbrakes = np.where(~traction_mask, ft * vv, 0.0)

    def time_avg(power):
        return float(np.sum(dt * power) / np.sum(dt))

    return {
        "dt": dt,
        "tt": tt,
        "ax": ax,
        "ay": ay,
        "k": curvature_by_point,
        "ptraction": ptraction,
        "pbrakes": pbrakes,
        "pdrag": pdrag,
        "pkinetic": pkinetic,
        "pmotor": pmotor,
        "pelectric": pelectric,
        "tmotor": tmotor,
        "wmotor": wmotor,
        "numotor": numotor,
        "avg_traction": time_avg(ptraction),
        "avg_brakes": time_avg(pbrakes),
        "avg_drag": time_avg(pdrag),
        "avg_motor": time_avg(pmotor),
        "avg_electric": time_avg(pelectric),
        "energy_j": float(np.sum(pelectric * dt)),
    }

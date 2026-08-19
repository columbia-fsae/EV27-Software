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

Stage 2 (`trace_speed_profile`/`_march`) is the hot loop. It used to be Numba-JIT-compiled,
but `tire`/`battery` are now plain Python objects (`Tire`/`Battery`, with SOC-dependent
lookups and CSV-backed state) rather than raw scalars/arrays, and Numba's nopython mode
can't call into arbitrary Python objects -- so JIT has been dropped here in favor of
correctness. (A `Tire`/`Battery` pair implemented as Numba `jitclass`es could bring it
back, but that's a bigger redesign than this fix.)
Everything here operates on plain NumPy arrays/scalars plus `tire`/`battery` objects
rather than the full `Car`/`Track` objects -- `lap_simulator.py` is the adapter that
unpacks those objects into the arguments these functions need.
"""
from __future__ import annotations

import numpy as np

GRAVITY = 9.806  # m/s^2, matches the original scripts' constant


def force_balance(
    v, k, mass, cg_height, wheelbase, cda, cla, air_density, tire,
    power_limit, ratio, count, efficiency, motor_w, motor_m, battery, t: float = None,
    battery_power_override: float = None,
):
    """Longitudinal brake/accel limits (m/s^2) at speed `v` and curvature `k`.

    Combines a friction-circle-derated grip limit with drag, then caps the achievable
    acceleration by whichever of (electrical power, battery power, weight-transfer-limited
    traction, motor torque curve) is most restrictive.

    `battery_power_override`, when given, replaces `battery.available_power()` as the
    battery's power ceiling for this call -- how `EcmsController` gates the pack down to
    less than its full physical limit without touching `battery`'s own state.
    """
    downforce = 0.5 * air_density * cla * v * v
    fz = GRAVITY * mass + downforce
    fy = v * v * k * mass

    potential_fx = tire.get_fx(fz, fy, mass)

    drag = 0.5 * air_density * cda * v * v
    brake = (-potential_fx - drag) / mass

    power_limited_accel = power_limit / mass / v if v > 0.0 else np.inf

    if battery is None:
        battery_limited_accel = np.inf
    elif battery_power_override is not None:
        battery_limited_accel = battery_power_override / mass / v if v > 0.0 else np.inf
    else:
        battery_limited_accel = battery.available_power() / mass / v if v > 0.0 else np.inf

    weight_transfer = (cg_height / wheelbase) * potential_fx / mass / GRAVITY
    traction_limited_accel = 0.5 * potential_fx / mass / (1.0 - weight_transfer)

    omega = v / tire.radius * ratio
    max_torque = np.interp(omega, motor_w, motor_m)
    motor_limited_accel = max_torque * count * ratio * efficiency / tire.radius / mass

    accel_limit = min(power_limited_accel, traction_limited_accel, motor_limited_accel, battery_limited_accel)
    accel = accel_limit - drag / mass
    return brake, accel


def corner_speed_limit(
    k, mass, cg_height, wheelbase, cda, cla, air_density, tire,
    power_limit, ratio, count, efficiency, motor, battery,
    v0=0.0, time_step=0.001, accel_tolerance=0.001 * GRAVITY, max_steps=100_000,
):
    """Steady-state cornering speed (m/s) for curvature `k`, found by time-marching.

    `v0` seeds the search (0.0 is a cold start); passing an already-known nearby speed
    lets this converge in a handful of steps instead of climbing all the way from a
    stop. `max_steps` is a safety net so a non-converging case can't hang.
    """
    v = v0
    a = accel_tolerance + 1.0  # ensure at least one iteration
    steps = 0
    while abs(a) > accel_tolerance and steps < max_steps:
        _, a = force_balance(
            v, k, mass, cg_height, wheelbase, cda, cla, air_density, tire,
            power_limit, ratio, count, efficiency, motor.torque_speed_w, motor.torque_speed_m, battery,
        )
        v = v + a * time_step
        steps += 1
    return v


def _march(
    direction, start_idx, v0, dx, t, limits_snapshot, max_steps, curvature_by_point,
    mass, cg_height, wheelbase, cda, cla, air_density, tire,
    power_limit, ratio, count, efficiency, motor_w, motor_m, battery,
):
    """Forward (accel) or backward (brake) march from `start_idx`, until the profile
    rejoins `limits_snapshot`. Returns a full-length array equal to `limits_snapshot`
    except along the visited stretch, plus the acceleration at the starting point.
    """
    n = limits_snapshot.shape[0]
    profile = limits_snapshot.copy()
    idx = start_idx
    v = v0
    step = dx if direction > 0 else -dx
    a0 = 0.0
    steps = 0

    while v <= limits_snapshot[idx] and steps < max_steps:
        k = curvature_by_point[idx]
        brake, accel = force_balance(
            v, k, mass, cg_height, wheelbase, cda, cla, air_density, tire,
            power_limit, ratio, count, efficiency, motor_w, motor_m, battery, t,
        )
        a = accel if direction > 0 else brake
        v_sq = v * v + 2.0 * a * step
        v = np.sqrt(v_sq) if v_sq > 0.0 else 0.0
        profile[idx] = v
        idx = (idx + direction) % n
        steps += 1
    return profile


def trace_speed_profile(
    point_limit, curvature_by_point, seg_idx_per_point, track_limits, dx,
    mass, cg_height, wheelbase, cda, cla, air_density, tire,
    power_limit, ratio, count, efficiency, motor,
):
    """The feasible speed trace around one lap, battery-blind (no `battery` param at
    all -- see `battery_forward_pass` for how a battery's acceleration limit gets
    layered on top afterward for cars that have one).

    `point_limit` is the per-point speed ceiling before considering accel/braking
    capability (corner grip limits and any externally-imposed segment limits).
    Wherever it steps up or down between consecutive points, an accel/brake profile is
    marched out from that transition and merged in via an elementwise minimum — the
    classic point-mass lap-sim technique.

    Returns `vv`.
    """
    n = point_limit.shape[0]
    vv = point_limit.copy()
    t = 0.0
    for i in range(n):
        p = (i - 1) % n
        max_steps = n-i
        if point_limit[p] < point_limit[i]:
            profile = _march(
                1, i, point_limit[p], dx, t, vv, max_steps, curvature_by_point,
                mass, cg_height, wheelbase, cda, cla, air_density, tire,
                power_limit, ratio, count, efficiency, motor.torque_speed_w, motor.torque_speed_m, None,
            )
            vv = np.minimum(vv, profile)
        elif point_limit[p] > point_limit[i]:
            profile = _march(
                -1, p, point_limit[i], dx, t, vv, max_steps, curvature_by_point,
                mass, cg_height, wheelbase, cda, cla, air_density, tire,
                power_limit, ratio, count, efficiency, motor.torque_speed_w, motor.torque_speed_m, None,
            )
            vv = np.minimum(vv, profile)
    return vv


def battery_forward_pass(
    vv_base, curvature_by_point, dx,
    mass, cg_height, wheelbase, cda, cla, air_density, tire,
    power_limit, ratio, count, efficiency, motor, battery, ecms=None,
):
    """Overlay battery-limited acceleration onto an already-resolved, battery-blind
    speed trace `vv_base` (no regen -- braking never depends on the battery, so
    `vv_base`'s braking zones are already final and this pass never needs to touch
    them).

    When `ecms` (an `EcmsController`) is given, the battery isn't simply granted its full
    physical `available_power()` at each point -- `vv_base`'s own point-to-point speed
    change already says what current the car would draw here if energy were free
    (`i_request`); ECMS minimizes its Hamiltonian to decide how much of that to actually
    grant, gated by the running price `ecms.lam`, and that gated power (not the raw
    physical ceiling) is what `force_balance` sees as the battery's limit for this point.

    A single forward integration, point by point: from each point's actual (possibly
    battery-reduced) speed, take one battery-aware acceleration step and cap the result
    at `vv_base[i]`, the true physical ceiling once the battery isn't binding. That cap
    is what keeps this from ever needing a backward march -- it's what makes it safe to
    step the battery's SOC/voltage/temperature sequentially as each point is finalized,
    unlike `trace_speed_profile`'s combined forward+backward version, where a backward
    march launched from a later point can retroactively lower an earlier point's speed
    *after* that point's battery current was already computed and locked in against the
    old, higher value.

    (An earlier version of this used discrete `_march` calls that stopped once they
    rejoined `vv_base`, mirroring `trace_speed_profile`'s structure. That's fragile here:
    when the battery isn't actually binding, this pass's independently-recomputed
    trajectory should retrace `vv_base` exactly, but tiny floating-point differences
    between the two made the march's `v <= vv_base[idx]` stopping check trip almost
    immediately, so no march ever covered more than one or two points -- leaving nearly
    every point looking "untouched" and forcing an expensive standalone
    `corner_speed_limit` solve at each one. Capping with `min()` every step instead of
    comparing for an exact rejoin sidesteps that entirely.)

    Returns `(vv, batt_i, batt_v, batt_soc, cell_t, batt_p_limit)`.
    """
    n = vv_base.shape[0]
    vv = vv_base.copy()
    batt_i = np.empty(n)
    batt_v = np.empty(n)
    batt_soc = np.empty(n)
    cell_t = np.empty(n)
    batt_p_limit = np.empty(n)

    for i in range(n):
        p = (i - 1) % n

        battery_power_override = None
        if ecms is not None:
            dt_i = dx / vv_base[i] if vv_base[i] > 0.0 else np.inf
            accel_request = (vv_base[i]**2 - vv_base[p]**2) / (2.0 * dx)
            f_drag_i = 0.5 * air_density * cda * vv_base[i]**2
            f_tract_request = accel_request * mass + f_drag_i
            power_request = f_tract_request * vv_base[i] if f_tract_request > 0.0 else 0.0
            i_request = power_request / battery.voltage

            i_ecms = ecms.command_current(battery, i_request, battery.batt_ocv, battery.batt_r0, dt_i)
            battery_power_override = i_ecms * battery.voltage

        _, accel = force_balance(
            vv[p], curvature_by_point[p], mass, cg_height, wheelbase, cda, cla, air_density, tire,
            power_limit, ratio, count, efficiency, motor.torque_speed_w, motor.torque_speed_m, battery,
            battery_power_override=battery_power_override,
        )
        v_sq = vv[p] * vv[p] + 2.0 * accel * dx
        v_candidate = np.sqrt(v_sq) if v_sq > 0.0 else 0.0
        vv[i] = min(v_candidate, vv_base[i])

        batt_i[i] = step_battery(battery, motor, vv, dx, mass, i, p, cda, air_density, efficiency, tire, ratio)
        batt_v[i] = battery.voltage
        batt_soc[i] = battery.soc
        cell_t[i] = battery.cell_T
        batt_p_limit[i] = battery.available_power()

        if ecms is not None:
            dt_actual = dx / vv[i] if vv[i] > 0.0 else np.inf
            ecms.update_s1(battery.soc, dx, dt_actual)
            ecms.update_s2(battery.cell_T, dx, dt_actual)

    return vv, batt_i, batt_v, batt_soc, cell_t, batt_p_limit


def step_battery(battery, motor, vv, dx, mass, i, p, cda, air_density, efficiency, tire, ratio):
    dt = dx/vv[i]
    ax = (vv[i] - vv[p])/dt
    f_drag = 0.5 * cda * air_density * vv[i]**2
    f_tract = ax * mass + f_drag
    power = f_tract * vv[i] if  f_tract > 0.0 else 0.0
    power = power / efficiency
    tmotor = f_tract * tire.radius / ratio / efficiency if f_tract > 0.0 else 0.0
    wmotor = vv[i] / tire.radius * ratio
    numotor = motor.efficiency(wmotor, tmotor)
    power_electric = power / numotor if f_tract > 0.0 else 0.0
    current = power_electric / battery.voltage
    battery.step(current, dt)
    return current


def power_and_energy(vv, dx, curvature_by_point, mass, cda, air_density, ratio,
                      efficiency, tire, motor, battery, batt_i, batt_v, batt_soc, cell_T,
                      batt_p_limit):
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
    tmotor = np.where(traction_mask, ft * tire.radius / ratio / efficiency, 0.0)
    wmotor = vv / tire.radius * ratio
    numotor = motor.efficiency(wmotor, tmotor)
    pelectric = np.where(traction_mask, pmotor / numotor, 0.0)
    pbrakes = np.where(~traction_mask, ft * vv, 0.0)

    batt_energy_usage = None if battery is None else float(np.sum(batt_i * batt_v * dt)) / 3.6e6  # J -> kWh

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
        "batt_energy": batt_energy_usage,
        "batt_i": batt_i,
        "batt_v": batt_v,
        "batt_soc": batt_soc,
        "cell_T": cell_T,
        "batt_p_limit": batt_p_limit,
    }

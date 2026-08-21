"""Tunes EcmsController's gains (s1_kp/s1_ki, s2_kp/s2_ki, s1_0/s2_0, k) against the
EV26B car from run_events.py, using a real 22-lap `run_multi_lap` per trial (ECMS pacing
is meaningless without genuine per-lap state carryover, same reasoning as
compare_packs.py).

A full joint grid over all ~7 parameters is intractable (each trial is a real 22-lap
sim); this instead stages the search, matching how you'd tune two coupled PI loops by
hand: sweep the SOC loop (s1_kp/s1_ki/s1_0) first, holding the thermal loop inert (very
high s2_max so it never binds), then verify/sweep the thermal loop (s2_kp/s2_ki/s2_0)
against an artificially-tight temperature budget (to force it to actually engage, since
this car doesn't get anywhere near 60 C under a lax budget) holding the SOC loop at its
best-found stage-1 settings. `k`/`k_temp` are swept alongside stage 1 since they set the
overall scale both loops operate at.
"""
import sys
import time
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lap_sim import ENDURANCE_EVENT, FSAE_EV, EcmsController, LapSimulator, grid_sweep
from lap_sim.ecms import linear_soc_schedule, linear_temp_schedule

sys.path.insert(0, str(Path(__file__).resolve().parent))
import run_events as re  # noqa: E402  (for build_ev27(), matching the real car this'll be used on)

LAPS = 22
DX_ENDURANCE = 0.5
TOTAL_DISTANCE = LAPS * ENDURANCE_EVENT.total_length
SOC_REF_FN = linear_soc_schedule(TOTAL_DISTANCE, soc_start=1.0)

# A schedule that's actually reachable: the no-ECMS baseline uses ~5.2 kWh over 22 laps
# at ~11 kW avg (per the original run_events.py output) in ~1664 s -- so SOC realistically
# won't hit exactly 0 at exactly 22 laps without either a tighter energy budget or slower
# pace than that baseline. Scoring below treats "close to empty, not grossly over/under"
# as good rather than demanding an exact 0.


TEMP_START = 30.0  # realistic pre-warmed/hot-day ambient, not the sim's cold-start 25 C default

# Measured for build_ev27()'s pack (140s3p, up from the earlier 105s4p) against the
# current cell data at SOC~1.0, T=30 C: Voc=580.2V, r0=0.001140 ohm -- pull fresh if the
# pack config OR the cell data changes. i_request from the battery-blind trace (also
# pack-voltage-dependent via i_request = power_request / battery.voltage, so it moved
# too even though the traction/aero physics didn't): max~125.8A, p90~106.5A,
# mean(nonzero)~49.2A.
PACK_VOC = 580.19
PACK_R0 = 0.001140
I_REQUEST_MAX = 125.75

# First sweep collapsed almost every trial: s1 could grow large enough that s1*Voc fully
# overwhelmed k*Ir, clipping i_star to exactly 0 -- with no floor above zero, force_balance
# then only allows deceleration (drag), the car stalls near v=0, dt=dx/v blows up at every
# remaining point, and the "lap time" becomes a ~16898s sim-grinding-to-a-halt artifact,
# not a real result. Fix: bound s1_max so that even at full saturation, i_star retains at
# least MIN_CURRENT_FRACTION of i_request at the worst-case (highest) request seen.
MIN_CURRENT_FRACTION = 0.3


def safe_s1_max(k: float) -> float:
    return (1.0 - MIN_CURRENT_FRACTION) * k * I_REQUEST_MAX / PACK_VOC


def k_temp_for(k: float) -> float:
    """Seeded so s2=1 shifts the denominator by ~10% of k -- comparable, O(1) footing to
    s1's numerator-based leverage. With the corrected +2*s2*k_temp*r0 denominator sign,
    H is unconditionally convex (no pole to avoid), so s2 can range as high as its PI
    gains push it; larger s2 just throttles current smoothly harder, asymptoting to 0."""
    return 0.05 * k / PACK_R0


def safe_s2_max(k: float, k_temp: float) -> float:
    """Denominator grows by a factor of (1 + 2*s2_max*k_temp*r0/k); caps s2_max so that
    factor maxes out at 5x (current throttled by up to 80% from thermal price alone)."""
    return 4.0 * k / (2 * k_temp * PACK_R0)


def run_trial(ecms_kwargs: dict, temp_end: float = 60.0) -> dict:
    car = re.build_ev27()
    car.battery._T = TEMP_START
    temp_ref_fn = linear_temp_schedule(TOTAL_DISTANCE, temp_start=TEMP_START, temp_end=temp_end)
    ecms = EcmsController(soc_ref_fn=SOC_REF_FN, temp_ref_fn=temp_ref_fn, **ecms_kwargs)
    endur_car = car.replace(ecms=ecms)
    t0 = time.time()
    try:
        endur = LapSimulator(FSAE_EV, ENDURANCE_EVENT, endur_car, dx=DX_ENDURANCE).run_multi_lap(LAPS)
    except Exception as exc:
        return {"ok": False, "error": str(exc), "elapsed": time.time() - t0}
    return {
        "ok": True, "elapsed": time.time() - t0,
        "lap_time": endur.lap_time, "final_soc": endur.stats.batt_soc[-1],
        "max_temp": endur.stats.cell_T.max(), "final_s1": ecms.s1, "final_s2": ecms.s2,
        "avg_kw": endur.stats.avg_electric / 1e3,
    }


def soc_score(final_soc: float) -> float:
    """Lower is better: penalizes both leaving energy unused (final_soc high) and
    running the pack past empty (shouldn't happen given the clip, but penalize anyway).
    Target band: 0-10% remaining.
    """
    if final_soc < 0.0:
        return 1e6
    if final_soc <= 0.10:
        return final_soc  # small reward for using nearly everything
    return final_soc * 5  # steeper penalty for leaving energy on the table


def stage1_soc_loop():
    print("[tune_ecms] Stage 1: SOC loop (s1_kp, s1_ki, s1_0) + k, thermal loop inert, "
          f"s1_max derived per-k to guarantee >= {MIN_CURRENT_FRACTION:.0%} of i_request always gets through...")
    k_values = [150.0, 200.0, 300.0]
    s1_kp_values = [10.0, 30.0, 60.0]
    s1_ki_values = [0.5, 2.0]
    s1_0_values = [1.0, 3.0]

    rows = []
    n_total = len(k_values) * len(s1_kp_values) * len(s1_ki_values) * len(s1_0_values)
    count = 0
    for k in k_values:
        s1_max = safe_s1_max(k)
        for s1_kp in s1_kp_values:
            for s1_ki in s1_ki_values:
                for s1_0 in s1_0_values:
                    count += 1
                    ecms_kwargs = dict(
                        k=k, k_temp=1.0,  # thermal loop inert this stage, k_temp irrelevant while s2_max~0
                        s1_kp=s1_kp, s1_ki=s1_ki, s1_0=s1_0, s1_max=s1_max,
                        s2_kp=0.0, s2_ki=0.0, s2_0=1.0, s2_max=1e-9,
                    )
                    result = run_trial(ecms_kwargs, temp_end=60.0)
                    print(f"[tune_ecms]  ({count}/{n_total}) k={k:.0f} (s1_max={s1_max:.1f}) s1_kp={s1_kp} s1_ki={s1_ki} s1_0={s1_0} "
                          f"-> {'OK' if result['ok'] else 'FAIL'} "
                          f"{'lap_time=%.0fs final_soc=%.3f' % (result['lap_time'], result['final_soc']) if result['ok'] else result.get('error')}")
                    rows.append({"k": k, "s1_max": s1_max, "s1_kp": s1_kp, "s1_ki": s1_ki, "s1_0": s1_0, **result})

    df = pd.DataFrame(rows)
    ok = df[df["ok"]].copy()
    if ok.empty:
        print("[tune_ecms] Stage 1: every trial failed.")
        return None, df
    ok["score"] = ok["lap_time"] + ok["final_soc"].apply(soc_score) * ok["lap_time"]
    best = ok.loc[ok["score"].idxmin()]
    print(f"[tune_ecms] Stage 1 best: k={best.k:.0f} s1_kp={best.s1_kp} s1_ki={best.s1_ki} s1_0={best.s1_0} "
          f"-> lap_time={best.lap_time:.0f}s final_soc={best.final_soc:.3f}")
    return best, df


def stage2_temp_loop(best_stage1):
    print("\n[tune_ecms] Stage 2: thermal loop (s2_kp, s2_ki, s2_0), SOC loop fixed at stage-1 best, "
          "temp_end lowered to 40 C to force it to actually engage...")
    k = best_stage1.k
    k_temp = k_temp_for(k)
    s2_max = safe_s2_max(k, k_temp)
    s2_kp_values = [0.05, 0.2, 1.0]
    s2_ki_values = [0.002, 0.01, 0.05]
    s2_0_values = [1.0, 3.0]

    rows = []
    n_total = len(s2_kp_values) * len(s2_ki_values) * len(s2_0_values)
    count = 0
    print(f"[tune_ecms]   k_temp={k_temp:.1f}, s2_max={s2_max:.2f} (pole at {2*s2_max:.2f})")
    for s2_kp in s2_kp_values:
        for s2_ki in s2_ki_values:
            for s2_0 in s2_0_values:
                count += 1
                ecms_kwargs = dict(
                    k=k, k_temp=k_temp,
                    s1_kp=best_stage1.s1_kp, s1_ki=best_stage1.s1_ki, s1_0=best_stage1.s1_0, s1_max=best_stage1.s1_max,
                    s2_kp=s2_kp, s2_ki=s2_ki, s2_0=s2_0, s2_max=s2_max,
                )
                result = run_trial(ecms_kwargs, temp_end=40.0)  # artificially tight to force engagement
                print(f"[tune_ecms]  ({count}/{n_total}) s2_kp={s2_kp} s2_ki={s2_ki} s2_0={s2_0} "
                      f"-> {'OK' if result['ok'] else 'FAIL'} "
                      f"{'lap_time=%.0fs max_temp=%.1fC final_s2=%.2f' % (result['lap_time'], result['max_temp'], result['final_s2']) if result['ok'] else result.get('error')}")
                rows.append({"s2_kp": s2_kp, "s2_ki": s2_ki, "s2_0": s2_0, **result})

    df = pd.DataFrame(rows)
    ok = df[df["ok"]].copy()
    if ok.empty:
        print("[tune_ecms] Stage 2: every trial failed.")
        return None, df
    # Want max_temp close to (at or just under) the 40 C artificial budget, without
    # collapsing lap_time entirely -- score by overshoot above budget, tie-broken by pace.
    ok["overshoot"] = (ok["max_temp"] - 40.0).clip(lower=0.0)
    ok["score"] = ok["overshoot"] * 1000 + ok["lap_time"]
    best = ok.loc[ok["score"].idxmin()]
    print(f"[tune_ecms] Stage 2 best: s2_kp={best.s2_kp} s2_ki={best.s2_ki} s2_0={best.s2_0} "
          f"-> max_temp={best.max_temp:.1f}C (budget 40C) lap_time={best.lap_time:.0f}s")
    return best, df


def main():
    best1, df1 = stage1_soc_loop()
    if best1 is None:
        return
    best2, df2 = stage2_temp_loop(best1)

    k_temp = k_temp_for(best1.k)
    print("\n[tune_ecms] ==== Recommended EcmsController defaults ====")
    print(f"k       = {best1.k:.1f}")
    print(f"k_temp  = {k_temp:.1f}")
    print(f"s1_kp   = {best1.s1_kp}")
    print(f"s1_ki   = {best1.s1_ki}")
    print(f"s1_0    = {best1.s1_0}")
    print(f"s1_max  = {best1.s1_max:.1f}")
    if best2 is not None:
        print(f"s2_kp   = {best2.s2_kp}")
        print(f"s2_ki   = {best2.s2_ki}")
        print(f"s2_0    = {best2.s2_0}")
        print(f"s2_max  = {safe_s2_max(best1.k, k_temp):.2f}")

    df1.to_csv(Path(__file__).parent / "tune_ecms_stage1.csv", index=False)
    if df2 is not None:
        df2.to_csv(Path(__file__).parent / "tune_ecms_stage2.csv", index=False)
    print("[tune_ecms] saved tune_ecms_stage1.csv / tune_ecms_stage2.csv")


if __name__ == "__main__":
    main()

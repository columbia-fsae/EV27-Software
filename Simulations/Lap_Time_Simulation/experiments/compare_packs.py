"""Battery pack size (series x parallel) design-space sweep: for every combination, size
the pack, scale the car's mass with it, pace the endurance run with `EcmsController`
against a full-22-lap SOC schedule, score the full FSAE event set, and plot total
competition points as a heatmap over series x parallel.

This is a genuinely expensive sweep: unlike `mass_power_grid_sweep.py` (which scores
endurance by running one lap and scaling the time by 22, fine when there's no battery
state to carry forward), ECMS pacing is meaningless without the pack's SOC actually
carrying across all 22 laps -- so endurance here is a real `run_multi_lap(22)` call per
grid point, ~22x the cost of the single-lap approximation. Keep `SERIES_VALUES` /
`PARALLEL_VALUES` small and `DX_ENDURANCE` coarse while iterating; tighten both once
you've confirmed the sweep runs in acceptable time.
"""
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lap_sim import (
    ACCEL_EVENT, EMRAX_208, EV25, ENDURANCE_EVENT, FSAE_EV, SKIDPAD_EVENT,
    Battery, CompetitionScorer, EcmsController, LapSimulator, grid_sweep, plotting,
)
from lap_sim.ecms import linear_soc_schedule
from lap_sim.vehicle import Aero, Car, Drivetrain, HighVoltageSystem

# --- knobs -------------------------------------------------------------------------
SERIES_VALUES = np.arange(100, 145, 11)     # cells in series to sweep
PARALLEL_VALUES = np.arange(2, 5, 1)        # cells in parallel to sweep
CELL_TYPE = "ampace_jp50"
OTHER_MASS_KG = 160 + 60                  # vehicle mass excluding the accumulator (chassis/driver/etc.)
LAPS = 22
DX_EVENT = 0.005                           # accel/skidpad resolution
DX_ENDURANCE = 0.5                         # endurance resolution -- the expensive one; coarsen further if too slow
SOC_DERATE = 0.0                            # 0 = linear SOC-vs-distance schedule; >0 = bows faster-than-linear late
ECMS_KP = 500.0
ECMS_KI = 20.0
ECMS_LAM_MAX = 400.0
# -------------------------------------------------------------------------------------


def build_car(series: int, parallel: int) -> Car:
    """The EV26B spec from run_events.py's `build_ev26b()` -- EV27 itself isn't usable
    here: its `drivetrain.ratio=1` direct-drive setup, `TIRE_LOOKUP` tire, and low-cla
    aero (vs. EV26B's 4.3:1 reduction, constant-mu tire, and high-downforce aero) leave
    it barely able to accelerate or corner, which tanked both this sweep's runtime
    (corner_speed_limit's time-marching grinds far longer to converge on a car that
    can't reach speed) and its scores. Swap this back to `EV27.replace(...)` once EV27's
    own spec (currently flagged TODO/placeholder in vehicle.py) is finalized.

    Sized per grid point: mass = chassis/driver mass + pack mass (per-cell mass x series
    x parallel), and `hv` updated to match so it doesn't go stale relative to the actual
    pack (though nothing in the physics reads `car.hv` -- it's tracked as data only, same
    as upstream `StaticVoltageBattery`'s docstring notes).
    """
    battery = Battery(series=series, parallel=parallel, cell_type=CELL_TYPE)
    pack_mass_kg = battery._m * series * parallel * 1.1
    return Car(
        name="EV26B (compare_packs)",
        mass=OTHER_MASS_KG + pack_mass_kg,
        cg=np.array([742.44, 0.0, 248.52]),
        aero=Aero(cda=1.7, cla=3.05),
        tire=EV25.tire,
        drivetrain=Drivetrain(motor=EMRAX_208, ratio=4.3, efficiency=0.96, count=1),
        hv=HighVoltageSystem(vmax=battery._max_v * series, vnom=battery._nom_v * series),
        l=1.530,
        battery=battery,
    )


def evaluate_pack(series: int, parallel: int) -> dict:
    scorer = CompetitionScorer()
    try:
        car = build_car(series, parallel)

        accel = LapSimulator(FSAE_EV, ACCEL_EVENT, car, dx=DX_EVENT).run()
        accel_time = accel.split_time(1, 2)

        skidpad = LapSimulator(FSAE_EV, SKIDPAD_EVENT, car, dx=DX_EVENT).run()
        skidpad_time = skidpad.split_time(2, 3)

        # ECMS is only attached for the endurance run below, not on `car` above --
        # otherwise accel/skidpad would also get gated by a controller (and its shared
        # lambda/distance-traveled state) calibrated for a 22-lap endurance budget, not
        # a 75 m sprint or a skidpad circle.
        total_distance = LAPS * ENDURANCE_EVENT.total_length
        soc_ref_fn = linear_soc_schedule(total_distance, soc_start=1.0, derate=SOC_DERATE)
        endur_car = car.replace(ecms=EcmsController(
            soc_ref_fn=soc_ref_fn, kp=ECMS_KP, ki=ECMS_KI, lam_max=ECMS_LAM_MAX,
        ))

        # Real per-lap state carryover (SOC, RC-branch sag, temperature, and the ECMS
        # controller's own lambda/distance) is the entire point of this sweep -- a
        # single-lap-scaled-by-22 shortcut (as CompetitionEvaluator/mass_power_grid_sweep.py
        # use) would make the SOC-pacing controller meaningless.
        endur = LapSimulator(FSAE_EV, ENDURANCE_EVENT, endur_car, dx=DX_ENDURANCE).run_multi_lap(LAPS)
        endur_time = endur.lap_time
        avg_power = endur.stats.avg_electric
        energy_wh = avg_power * endur_time / 3600.0
        autocross_time = endur_time * 0.8 / LAPS
        final_soc = endur.stats.batt_soc[-1]

        score = scorer.score(accel_time, skidpad_time, autocross_time, endur_time, energy_wh)

        return {
            "mass_kg": car.mass, "accel_time": accel_time, "skidpad_time": skidpad_time,
            "endur_time": endur_time, "energy_wh": energy_wh, "final_soc": final_soc,
            "total_points": score.total, "score_breakdown": score,
        }
    except Exception as exc:
        print(f"[compare_packs]   series={series} parallel={parallel} FAILED: {exc}")
        return {
            "mass_kg": np.nan, "accel_time": np.nan, "skidpad_time": np.nan,
            "endur_time": np.nan, "energy_wh": np.nan, "final_soc": np.nan,
            "total_points": np.nan, "score_breakdown": None,
        }


def main():
    n_combos = len(SERIES_VALUES) * len(PARALLEL_VALUES)
    print(f"[compare_packs] sweeping {len(SERIES_VALUES)} x {len(PARALLEL_VALUES)} = "
          f"{n_combos} pack configurations (each a full 22-lap endurance run)...")

    def evaluate(series, parallel):
        print(f"[compare_packs] series={series} parallel={parallel} ...")
        result = evaluate_pack(int(series), int(parallel))
        pts = result["total_points"]
        print(f"[compare_packs]   -> mass={result['mass_kg']:.1f} kg, points={pts:.1f}, score_breakdown = {result['score_breakdown']}, accel_time={result['accel_time']}, skidpad_time = {result['skidpad_time']}, endur=time = {result['endur_time']}"
              if not np.isnan(pts) else "[compare_packs]   -> failed, see above")
        return result

    df = grid_sweep({"series": SERIES_VALUES, "parallel": PARALLEL_VALUES}, evaluate)
    print(df[["series", "parallel", "mass_kg", "final_soc", "total_points"]])

    points_pivot = df.pivot(index="parallel", columns="series", values="total_points")
    x_vals, y_vals = points_pivot.columns.values, points_pivot.index.values

    plotting.plot_heatmap(
        x_vals, y_vals, points_pivot.values, "Series cells", "Parallel cells",
        "Total points", "Competition Points vs. Pack Configuration",
    )

    best = df.loc[df["total_points"].idxmax()]
    print(f"[compare_packs] best: series={int(best.series)} parallel={int(best.parallel)} "
          f"-> {best.total_points:.1f} points (mass={best.mass_kg:.1f} kg, "
          f"final SOC={best.final_soc:.2%})")

    plt.show()


if __name__ == "__main__":
    main()

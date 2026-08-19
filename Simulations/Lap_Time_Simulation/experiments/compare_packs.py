import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lap_sim import (
    ACCEL_EVENT, EMRAX_208, EV25, ENDURANCE_EVENT, FSAE_EV, SKIDPAD_EVENT,
    Battery, CompetitionScorer, EcmsController, LapSimulator, grid_sweep, plotting,
)
from lap_sim.ecms import linear_soc_schedule, linear_temp_schedule
from lap_sim.vehicle import Aero, Car, Drivetrain, HighVoltageSystem

# --- knobs -------------------------------------------------------------------------
SERIES_VALUES = np.arange(100, 145, 4)     # cells in series to sweep
PARALLEL_VALUES = np.arange(2, 5, 1)        # cells in parallel to sweep
MASS_VALUES = np.arange(200, 301, 10)
CELL_TYPE = "ampace_jp50"
OTHER_MASS_KG = 160 + 60                  # vehicle mass excluding the accumulator (chassis/driver/etc.)
LAPS = 22
DX_EVENT = 0.005                           # accel/skidpad resolution
DX_ENDURANCE = 0.5                         # endurance resolution -- the expensive one; coarsen further if too slow
SOC_DERATE = 0.0                            # 0 = linear SOC-vs-distance schedule; >0 = bows faster-than-linear late
TEMP_START = 30.0  # realistic pre-warmed/hot-day ambient, not the sim's cold-start 25 C default
TEMP_END = 60.0
TEMP_DERATE = 0.0
# Tuned via experiments/tune_ecms.py's staged sweep against the 144s2p pack -- k_temp
# especially depends on r0 (see EcmsController's module docstring: "seed near k/r0"),
# which shifts with parallel, so these are a reasonable starting point across the grid
# rather than independently optimal for every series/parallel combo swept below.
ECMS_K = 300.0
ECMS_K_TEMP = 26690.4
ECMS_S1_KP = 10.0
ECMS_S1_KI = 2.0
ECMS_S1_MAX = 51.6
ECMS_S2_KP = 0.05
ECMS_S2_KI = 0.01
ECMS_S2_MAX = 40.0
ENERGY_METRIC = "final"  # "final" = actual energy consumed over the 22 laps (recommended);
                          # "nominal" = the pack's rated capacity from battery.py (even across
                          # mass, since it doesn't depend on mass, but ignores how efficiently
                          # that capacity actually got used)
# -------------------------------------------------------------------------------------


def build_car(series: int, parallel: int, mass: int) -> Car:
    battery = Battery(series=series, parallel=parallel, cell_type=CELL_TYPE)
    return Car(
        name="EV26B (compare_packs)",
        mass=mass,
        cg=np.array([742.44, 0.0, 248.52]),
        aero=Aero(cda=1.7, cla=3.05),
        tire=EV25.tire,
        drivetrain=Drivetrain(motor=EMRAX_208, ratio=4.3, efficiency=0.96, count=1),
        hv=HighVoltageSystem(vmax=battery._max_v * series, vnom=battery._nom_v * series),
        l=1.530,
        battery=battery,
    )


def evaluate_pack(pack_combo: tuple[int], mass: int) -> dict:
    scorer = CompetitionScorer()
    series, parallel = int(pack_combo[0]), int(pack_combo[1])
    try:
        car = build_car(series, parallel, mass)
        car.battery._T = TEMP_START  # realistic pre-warmed/hot-day start, not the sim's cold-start 25 C default

        accel = LapSimulator(FSAE_EV, ACCEL_EVENT, car, dx=DX_EVENT).run()
        accel_time = accel.split_time(1, 2)

        skidpad = LapSimulator(FSAE_EV, SKIDPAD_EVENT, car, dx=DX_EVENT).run()
        skidpad_time = skidpad.split_time(2, 3)

        # ECMS is only attached for the endurance run below, not on `car` above --
        # otherwise accel/skidpad would also get gated by a controller (and its shared
        # s1/s2/distance-traveled state) calibrated for a 22-lap endurance budget, not
        # a 75 m sprint or a skidpad circle.
        total_distance = LAPS * ENDURANCE_EVENT.total_length
        soc_ref_fn = linear_soc_schedule(total_distance, soc_start=1.0, derate=SOC_DERATE)
        temp_ref_fn = linear_temp_schedule(total_distance, temp_start=TEMP_START, temp_end=TEMP_END, derate=TEMP_DERATE)
        endur_car = car.replace(ecms=EcmsController(
            soc_ref_fn=soc_ref_fn, temp_ref_fn=temp_ref_fn, k=ECMS_K, k_temp=ECMS_K_TEMP,
            s1_kp=ECMS_S1_KP, s1_ki=ECMS_S1_KI, s1_max=ECMS_S1_MAX,
            s2_kp=ECMS_S2_KP, s2_ki=ECMS_S2_KI, s2_max=ECMS_S2_MAX,
        ))

        # Real per-lap state carryover (SOC, RC-branch sag, temperature, and the ECMS
        # controller's own s1/s2/distance) is the entire point of this sweep -- a
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
            "series": series, "parallel": parallel,
            "mass_kg": car.mass, "accel_time": accel_time, "skidpad_time": skidpad_time,
            "endur_time": endur_time, "energy_wh": energy_wh, "final_soc": final_soc,
            "pack_energy_kwh": car.battery.pack_energy, "final_energy_kwh": endur.stats.batt_energy,
            "total_points": score.total, "score_breakdown": score,
        }
    except Exception as exc:
        print(f"[compare_packs]   series={series} parallel={parallel} mass={mass} FAILED: {exc}")
        return {
            "series": series, "parallel": parallel,
            "mass_kg": np.nan, "accel_time": np.nan, "skidpad_time": np.nan,
            "endur_time": np.nan, "energy_wh": np.nan, "final_soc": np.nan,
            "pack_energy_kwh": np.nan, "final_energy_kwh": np.nan,
            "total_points": np.nan, "score_breakdown": None,
        }


def main():
    pack_combos = np.stack(np.meshgrid(SERIES_VALUES, PARALLEL_VALUES), axis=-1).reshape(-1, 2)
    n_combos = len(pack_combos) * len(MASS_VALUES)
    print(f"[compare_packs] sweeping {len(pack_combos)} pack configs x {len(MASS_VALUES)} masses = "
          f"{n_combos} configurations (each a full 22-lap endurance run)...")

    def evaluate(pack_combo, mass):
        print(f"[compare_packs] series={pack_combo[0]} parallel={pack_combo[1]} mass={mass} ...")
        result = evaluate_pack(pack_combo, int(mass))
        pts = result["total_points"]
        print(f"[compare_packs]   -> mass={result['mass_kg']:.1f} kg, points={pts:.1f}, score_breakdown = {result['score_breakdown']}, accel_time={result['accel_time']}, skidpad_time = {result['skidpad_time']}, endur=time = {result['endur_time']}"
              if not np.isnan(pts) else "[compare_packs]   -> failed, see above")
        return result

    df = grid_sweep({"pack_combo": pack_combos, "mass": MASS_VALUES}, evaluate)
    print(df[["series", "parallel", "mass_kg", "final_soc", "total_points",
               "energy_wh", "pack_energy_kwh", "final_energy_kwh"]])

    csv_path = Path(__file__).with_name("compare_packs_results.csv")
    df.to_csv(csv_path, index=False)
    print(f"[compare_packs] saved full results to {csv_path}")

    df = df.dropna(subset=["total_points"])

    energy_col = "final_energy_kwh" if ENERGY_METRIC == "final" else "pack_energy_kwh"
    energy_wh = df[energy_col] * 1000.0  # kWh -> Wh, more readable scale

    # --- grid 1: series x parallel -> points, best achieved across the mass sweep -----
    # (mass is now a third swept dimension, so a plain series/parallel pivot needs an
    # aggregate over it rather than one value per cell; `.pivot()` would also just error
    # on the resulting duplicate (series, parallel) rows)
    sp_pivot = df.pivot_table(index="parallel", columns="series", values="total_points", aggfunc="max")
    plotting.plot_heatmap(
        sp_pivot.columns.values, sp_pivot.index.values, sp_pivot.values,
        "Series cells", "Parallel cells", "Total points (best over mass sweep)",
        "Competition Points vs. Pack Configuration",
    )

    # --- grid 2: mass x energy -> points --------------------------------------------
    # Energy is a simulation *output*, not a swept variable, so (mass, energy) pairs
    # don't land on a regular grid -- tricontourf interpolates a filled contour over the
    # actual (irregular) sample cloud instead of assuming even spacing.
    fig2, ax2 = plt.subplots(figsize=(8, 6))
    contour = ax2.tricontourf(energy_wh, df["mass_kg"], df["total_points"], levels=20, cmap="viridis")
    ax2.scatter(energy_wh, df["mass_kg"], c="k", s=6, alpha=0.3)
    ax2.set_xlabel(f"{'Endurance energy consumed' if ENERGY_METRIC == 'final' else 'Nominal pack energy'} (Wh)")
    ax2.set_ylabel("Mass (kg)")
    ax2.set_title("Competition Points vs. Mass and Energy")
    fig2.colorbar(contour, ax=ax2, label="Total points")

    # --- 3d: energy x mass x pack size, colored by points ---------------------------
    fig3 = plt.figure(figsize=(9, 7))
    ax3 = fig3.add_subplot(projection="3d")
    cell_count = df["series"] * df["parallel"]  # collapses (series, parallel) to one spatial axis
    scatter3d = ax3.scatter(energy_wh, df["mass_kg"], cell_count, c=df["total_points"], cmap="viridis")
    ax3.set_xlabel(f"{'Endurance energy consumed' if ENERGY_METRIC == 'final' else 'Nominal pack energy'} (Wh)")
    ax3.set_ylabel("Mass (kg)")
    ax3.set_zlabel("Cells in pack (series x parallel)")
    ax3.set_title("Competition Points across Energy, Mass, and Pack Size")
    fig3.colorbar(scatter3d, ax=ax3, shrink=0.6, label="Total points")

    best = df.loc[df["total_points"].idxmax()]
    print(f"[compare_packs] best: series={int(best.series)} parallel={int(best.parallel)} "
          f"-> {best.total_points:.1f} points (mass={best.mass_kg:.1f} kg, "
          f"final SOC={best.final_soc:.2%})")

    plt.show()


if __name__ == "__main__":
    main()

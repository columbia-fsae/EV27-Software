import os
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lap_sim import (
    ACCEL_EVENT, EMRAX_208, EV25, EV27, ENDURANCE_EVENT, FSAE_EV, SKIDPAD_EVENT,
    Battery, CompetitionScorer, EcmsController, LapSimulator, grid_sweep, plotting,
)
from lap_sim.ecms import linear_soc_schedule, linear_temp_schedule
from lap_sim.vehicle import Aero, Car, Drivetrain, HighVoltageSystem

#  knobs 
SERIES_VALUES = np.arange(100, 145, 4)     # cells in series to sweep
PARALLEL_VALUES = np.arange(2, 6, 1)        # cells in parallel to sweep
MASS_VALUES = np.arange(200, 301, 5)
CELL_TYPE = "ampace_jp50"
OTHER_MASS_KG = 160 + 60                  # vehicle mass excluding the accumulator (chassis/driver/etc.)
LAPS = 22
DX_EVENT = 0.005                           # accel/skidpad resolution
DX_ENDURANCE = 0.5                         # endurance resolution
# Process pool size for the grid sweep below -- each grid cell (~14s) is fully
# independent, so this parallelizes near-linearly with physical core count. Set to 1
# to run sequentially (e.g. for debugging a single cell's traceback without it getting
# swallowed/reordered by a worker process).
N_WORKERS = max(1, (os.cpu_count() or 1) - 1)
SOC_DERATE = 0.0                           # 0 = linear SOC-vs-distance schedule; >0 = bows faster-than-linear late
TEMP_START = 30.0  # realistic pre-warmed/hot-day ambient, not the sim's cold-start 25 C default
TEMP_END = 60.0
TEMP_DERATE = 0.0
# Tuned via experiments/tune_ecms.py's staged sweep against build_ev26b()'s 105s4p pack,
# the current cell data (hppc_new_cells_full_0819_fitted_parameters_CORRECTED.csv), AND
# Cell's convective cooling term (see cell.py's _COOLING_H_W_M2K) 
REDUCED_COOLING = True  # matches cell.py's current _COOLING_H_W_M2K = 20.0

_ECMS_GAINS_NOMINAL = dict(
    k=200.0, k_temp=15600.6, s1_kp=10.0, s1_ki=2.0, s1_max=66.1,
    s2_kp=0.2, s2_ki=0.002, s2_max=40.0, s2_0=1.0,
)
_ECMS_GAINS_REDUCED_COOLING = dict(
    k=300.0, k_temp=13157.9, s1_kp=10.0, s1_ki=0.5, s1_max=45.5,
    s2_kp=0.7, s2_ki=0.0015, s2_max=40.0, s2_0=1.0,
)
_ecms_gains = _ECMS_GAINS_REDUCED_COOLING if REDUCED_COOLING else _ECMS_GAINS_NOMINAL
ECMS_K = _ecms_gains["k"]
ECMS_K_TEMP = _ecms_gains["k_temp"]
ECMS_S1_KP = _ecms_gains["s1_kp"]
ECMS_S1_KI = _ecms_gains["s1_ki"]
ECMS_S1_MAX = _ecms_gains["s1_max"]
ECMS_S2_KP = _ecms_gains["s2_kp"]
ECMS_S2_KI = _ecms_gains["s2_ki"]
ECMS_S2_MAX = _ecms_gains["s2_max"]
ECMS_S2_0 = _ecms_gains["s2_0"]
ENERGY_METRIC = "final"  # "final" = actual energy consumed over the 22 laps
                          # "nominal" = the pack's rated capacity from battery.py (even across
                          # mass, since it doesn't depend on mass, but ignores how efficiently
                          # that capacity actually got used)
FIG4_N_COMBOS = 10                    # series/parallel combos sampled across the full pack-energy range
FIG4_CELL_MASS_KG = 0.07             # per-cell mass, used to build each pack's total vehicle mass
FIG4_NON_BATTERY_MASS_KG = 175 + 60  # chassis/vehicle w/o battery (175 kg) + driver (60 kg)
FIG4_MASS_OFFSETS_KG = (0, 2, 4, 6)    # error-bar samples: base mass, +5 kg, +10 kg


def build_car(series: int, parallel: int, mass: int) -> Car:
    battery = Battery(series=series, parallel=parallel, cell_type=CELL_TYPE)
    return Car(
        name="EV26B (compare_packs)",
        mass=mass,
        cg=np.array([742.44, 0.0, 248.52]),
        aero=Aero(cda=1.7, cla=3.05),
        tire=EV27.tire,
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

        # ECMS is only attached for the endurance run below, not on `car` 
        total_distance = LAPS * ENDURANCE_EVENT.total_length
        soc_ref_fn = linear_soc_schedule(total_distance, soc_start=1.0, derate=SOC_DERATE)
        temp_ref_fn = linear_temp_schedule(total_distance, temp_start=TEMP_START, temp_end=TEMP_END, derate=TEMP_DERATE)
        endur_car = car.replace(ecms=EcmsController(
            soc_ref_fn=soc_ref_fn, temp_ref_fn=temp_ref_fn, k=ECMS_K, k_temp=ECMS_K_TEMP,
            s1_kp=ECMS_S1_KP, s1_ki=ECMS_S1_KI, s1_max=ECMS_S1_MAX,
            s2_kp=ECMS_S2_KP, s2_ki=ECMS_S2_KI, s2_max=ECMS_S2_MAX, s2_0=ECMS_S2_0,
        ))

        # Real per-lap state carryover (SOC, RC-branch sag, temperature, and the ECMS
        # controller's own s1/s2/distance)
        endur = LapSimulator(FSAE_EV, ENDURANCE_EVENT, endur_car, dx=DX_ENDURANCE).run_multi_lap(LAPS)
        endur_time = endur.lap_time
        avg_power = endur.stats.avg_electric
        energy_wh = avg_power * endur_time / 3600.0
        autocross_time = endur_time * 0.8 / LAPS
        final_soc = endur.stats.batt_soc[-1]

        score = scorer.score(accel_time, skidpad_time, autocross_time, endur_time, energy_wh)

        return {
            "series": series, "parallel": parallel,
            "mass_kg": car.mass, "batt_mass": car.battery._batt_mass,"accel_time": accel_time, "skidpad_time": skidpad_time,
            "endur_time": endur_time, "energy_wh": energy_wh, "final_soc": final_soc,
            "pack_energy_kwh": car.battery.pack_energy, "final_energy_kwh": endur.stats.batt_energy,
            "total_points": score.total, "score_breakdown": score,
        }
    except Exception as exc:
        print(f"[compare_packs]   series={series} parallel={parallel} mass={mass} FAILED: {exc}")
        return {
            "series": series, "parallel": parallel,
            "mass_kg": np.nan, "batt_mass": np.nan, "accel_time": np.nan, "skidpad_time": np.nan,
            "endur_time": np.nan, "energy_wh": np.nan, "final_soc": np.nan,
            "pack_energy_kwh": np.nan, "final_energy_kwh": np.nan,
            "total_points": np.nan, "score_breakdown": None,
        }


def evaluate_grid_cell(pack_combo, mass):
    """`grid_sweep`'s per-cell callback -- module-level (rather than a closure inside
    `main()`) so it can be pickled and shipped to worker processes when N_WORKERS > 1.
    """
    print(f"[compare_packs] series={pack_combo[0]} parallel={pack_combo[1]} mass={mass} ...")
    result = evaluate_pack(pack_combo, int(mass))
    pts = result["total_points"]
    print(f"[compare_packs]   -> mass={result['mass_kg']:.1f} kg, points={pts:.1f}, score_breakdown = {result['score_breakdown']}, accel_time={result['accel_time']}, skidpad_time = {result['skidpad_time']}, endur=time = {result['endur_time']}"
          if not np.isnan(pts) else "[compare_packs]   -> failed, see above")
    return result


def main():
    pack_combos = np.stack(np.meshgrid(SERIES_VALUES, PARALLEL_VALUES), axis=-1).reshape(-1, 2)
    n_combos = len(pack_combos) * len(MASS_VALUES)
    print(f"[compare_packs] sweeping {len(pack_combos)} pack configs x {len(MASS_VALUES)} masses = "
          f"{n_combos} configurations (each a full 22-lap endurance run) across {N_WORKERS} worker process(es)...")

    df = grid_sweep(
        {"pack_combo": pack_combos, "mass": MASS_VALUES}, evaluate_grid_cell, n_workers=N_WORKERS,
    )
    print(df[["series", "parallel", "mass_kg", "final_soc", "total_points",
               "energy_wh", "pack_energy_kwh", "final_energy_kwh"]])

    csv_path = Path(__file__).with_name("compare_packs_results.csv")
    df.to_csv(csv_path, index=False)
    print(f"[compare_packs] saved full results to {csv_path}")

    df = df.dropna(subset=["total_points"])

    energy_col = "final_energy_kwh" if ENERGY_METRIC == "final" else "pack_energy_kwh"
    energy_wh = df[energy_col] * 1000.0  # kWh -> Wh, more readable scale

    # grid 1: series x parallel -> points, best achieved across the mass sweep 
    sp_pivot = df.pivot_table(index="parallel", columns="series", values="total_points", aggfunc="max")
    plotting.plot_heatmap(
        sp_pivot.columns.values, sp_pivot.index.values, sp_pivot.values,
        "Series cells", "Parallel cells", "Total points (best over mass sweep)",
        "Competition Points vs. Pack Configuration",
    )

    # grid 2: mass x energy -> points
    fig2, ax2 = plt.subplots(figsize=(8, 6))
    contour = ax2.tricontourf(energy_wh, df["mass_kg"], df["total_points"], levels=20, cmap="viridis")
    ax2.scatter(energy_wh, df["mass_kg"], c="k", s=6, alpha=0.3)
    ax2.set_xlabel(f"{'Endurance energy consumed' if ENERGY_METRIC == 'final' else 'Nominal pack energy'} (Wh)")
    ax2.set_ylabel("Mass (kg)")
    ax2.set_title("Competition Points vs. Mass and Energy")
    fig2.colorbar(contour, ax=ax2, label="Total points")

    # 3d: energy x mass x pack size, colored by points
    fig3 = plt.figure(figsize=(9, 7))
    ax3 = fig3.add_subplot(projection="3d")
    cell_count = df["series"] * df["parallel"]  # collapses (series, parallel) to one spatial axis
    scatter3d = ax3.scatter(energy_wh, df["mass_kg"], cell_count, c=df["total_points"], cmap="viridis")
    ax3.set_xlabel(f"{'Endurance energy consumed' if ENERGY_METRIC == 'final' else 'Nominal pack energy'} (Wh)")
    ax3.set_ylabel("Mass (kg)")
    ax3.set_zlabel("Cells in pack (series x parallel)")
    ax3.set_title("Competition Points across Energy, Mass, and Pack Size")
    fig3.colorbar(scatter3d, ax=ax3, shrink=0.6, label="Total points")

    # fig 4: points vs. energy for a handful of packs spanning the full energy range,
    # with error bars built from a small mass sweep (base weight, +2/+4/+6 kg) around
    # each pack's actual expected mass (175 kg non-battery + 60 kg driver + cell mass).
    combo_energy = (
        df.groupby(["series", "parallel"])["pack_energy_kwh"].first()
        .reset_index().sort_values("pack_energy_kwh").reset_index(drop=True)
    )
    pick_idx = sorted(set(np.linspace(0, len(combo_energy) - 1, FIG4_N_COMBOS).round().astype(int)))
    selected_combos = combo_energy.loc[pick_idx]

    fig4, ax4 = plt.subplots(figsize=(8, 6))
    fig4_records = []  # full mass-sweep samples, kept around for fig 5
    for _, row in selected_combos.iterrows():
        series, parallel = int(row.series), int(row.parallel)
        label = f"{series}s{parallel}p"
        design_e = row.pack_energy_kwh * 1000.0
        base_mass = FIG4_NON_BATTERY_MASS_KG + series * parallel * FIG4_CELL_MASS_KG

        try:
            masses = [base_mass + off for off in FIG4_MASS_OFFSETS_KG]
            samples = [evaluate_pack((series, parallel), int(round(m))) for m in masses]
            if any(np.isnan(s["total_points"]) for s in samples):
                raise ValueError("a mass sample failed to simulate")
            sample_energy = np.array([s[energy_col] for s in samples]) * 1000.0
            sample_points = np.array([s["total_points"] for s in samples])
            mean_energy, mean_points = sample_energy.mean(), sample_points.mean()
            e_err = [[mean_energy - sample_energy.min()], [sample_energy.max() - mean_energy]]
            p_err = [[mean_points - sample_points.min()], [sample_points.max() - mean_points]]
            ax4.errorbar(mean_energy, mean_points, xerr=e_err, yerr=p_err,
                         fmt="o", capsize=4, label=label)
            ax4.annotate(f"{design_e:.0f} Wh", (mean_energy, mean_points),
                         textcoords="offset points", xytext=(6, 6), fontsize=7)
            fig4_records.append({
                "label": label, "design_e": design_e,
                "masses": np.array(masses), "points": sample_points,
            })
        except Exception as exc:
            print(f"[compare_packs] fig4: mass error bars failed for {label} ({exc}); "
                  "falling back to a single point, no error bars")
            single = evaluate_pack((series, parallel), int(round(base_mass)))
            if np.isnan(single["total_points"]):
                print(f"[compare_packs] fig4: {label} failed outright, skipping")
                continue
            ax4.scatter(single[energy_col] * 1000.0, single["total_points"], s=40, label=label)
            ax4.annotate(f"{design_e:.0f} Wh", (single[energy_col] * 1000.0, single["total_points"]),
                         textcoords="offset points", xytext=(6, 6), fontsize=7)

    ax4.set_xlabel(f"{'Endurance energy consumed' if ENERGY_METRIC == 'final' else 'Nominal pack energy'} (Wh)")
    ax4.set_ylabel("Total points")
    ax4.set_title(
        "Competition Points vs. Energy Across Representative Pack Configurations\n"
        f"(error bars: {FIG4_NON_BATTERY_MASS_KG} kg + cell mass, "
        f"+{FIG4_MASS_OFFSETS_KG[1]} to +{FIG4_MASS_OFFSETS_KG[-1]} kg; point labels = nameplate energy)"
    )
    ax4.legend(fontsize=8)

    # fig 5: for every fixed pack design (each with its own single nameplate energy
    # number), one line per pack, so designs can be compared side by side.
    if fig4_records:
        fig5, ax5 = plt.subplots(figsize=(8, 6))
        for rec in fig4_records:
            line, = ax5.plot(rec["masses"], rec["points"], "o-",
                              label=f"{rec['label']} (~{rec['design_e']:.0f} Wh)")
            for m, p in zip(rec["masses"], rec["points"]):
                ax5.annotate(f"{p:.1f}", (m, p), textcoords="offset points", xytext=(0, 6),
                             fontsize=7, ha="center", color=line.get_color())
        ax5.set_xlabel("Mass (kg)")
        ax5.set_ylabel("Total points")
        ax5.set_title("Points vs. Mass, per Fixed Pack Design (one line per nameplate energy)")
        ax5.legend(fontsize=8)
    else:
        print("[compare_packs] fig5: no combo had a full mass sweep to show mass/points variance for; skipping")

    best = df.loc[df["total_points"].idxmax()]
    print(f"[compare_packs] best: series={int(best.series)} parallel={int(best.parallel)} "
          f"-> {best.total_points:.1f} points (mass={best.mass_kg:.1f} kg, "
          f"final SOC={best.final_soc:.2%})")

    plt.show()


if __name__ == "__main__":
    main()

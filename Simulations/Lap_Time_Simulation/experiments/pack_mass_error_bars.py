import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import compare_packs as cp
from lap_sim import Battery

#  knobs
# Set to an explicit list of (series, parallel) tuples to test only those packs, e.g.:
#   SELECTED_PACKS = [(104, 3), (116, 3), (128, 4)]
# Leave as None to auto-select N_AUTO_PACKS packs spanning the full nameplate-energy
# range of compare_packs.py's SERIES_VALUES x PARALLEL_VALUES grid (same selection
# compare_packs.py's fig 4 uses, just without running the full grid sweep first).
SELECTED_PACKS = [(100, 2), (128, 2), (104, 3), (124, 3), (144, 3), (124, 4), (120, 5)] #[(100, 2), (130, 2), (100, 3), (110, 3), (120, 3), (130, 3), (144, 3), (105, 4), (112, 4), (130, 4), (100, 5), (112, 5), (124, 5), (144, 5)]
N_AUTO_PACKS = cp.FIG4_N_COMBOS  # only used when SELECTED_PACKS is None


def select_packs(n: int) -> list[tuple[int, int]]:
    """Pick `n` (series, parallel) combos spanning the full nameplate pack-energy range
    of compare_packs.py's sweep grid, without running any simulations."""
    combos = np.stack(np.meshgrid(cp.SERIES_VALUES, cp.PARALLEL_VALUES), axis=-1).reshape(-1, 2)
    energies = np.array([
        Battery(series=int(s), parallel=int(p), cell_type=cp.CELL_TYPE).pack_energy for s, p in combos
    ])
    order = np.argsort(energies)
    pick_idx = sorted(set(np.linspace(0, len(order) - 1, n).round().astype(int)))
    return [(int(combos[order[i]][0]), int(combos[order[i]][1])) for i in pick_idx]


def main():
    packs = SELECTED_PACKS if SELECTED_PACKS is not None else select_packs(N_AUTO_PACKS)
    print(f"[pack_mass_error_bars] testing {len(packs)} pack(s): {packs}")

    energy_col = "final_energy_kwh" if cp.ENERGY_METRIC == "final" else "pack_energy_kwh"

    fig4, ax4 = plt.subplots(figsize=(8, 6))
    fig6, ax6 = plt.subplots(figsize=(8, 6))
    fig7, ax7 = plt.subplots(figsize=(8, 6))

    fig4_records = []  # full mass-sweep samples, kept around for fig 5
    prev_accel = 0
    prev_skidpad = 0
    prev_endur = 0
    prev_accel_weight = 0
    prev_skidpad_weight = 0
    prev_endur_weight = 0
    for i, (series, parallel) in enumerate(packs):
        label = f"{series}s{parallel}p"
        design_e = Battery(series=series, parallel=parallel, cell_type=cp.CELL_TYPE).pack_energy * 1000.0
        base_mass = cp.FIG4_NON_BATTERY_MASS_KG + series * parallel * cp.FIG4_CELL_MASS_KG

        try:
            masses = [base_mass + off for off in cp.FIG4_MASS_OFFSETS_KG]
            samples = [cp.evaluate_pack((series, parallel), int(round(m))) for m in masses]
            if any(np.isnan(s["total_points"]) for s in samples):
                raise ValueError("a mass sample failed to simulate")
            sample_energy = np.array([s[energy_col] for s in samples]) * 1000.0
            sample_points = np.array([s["total_points"] for s in samples])
            accel_points = np.array([s["score_breakdown"].accel for s in samples])
            skidpad_points = np.array([s["score_breakdown"].skidpad for s in samples])
            endur_points = np.array([s["score_breakdown"].endurance for s in samples])
            mean_energy, mean_points, mean_accel, mean_skidpad, mean_endur = sample_energy.mean(), sample_points.mean(), accel_points.mean(), skidpad_points.mean(), endur_points.mean()
            e_err = [[mean_energy - sample_energy.min()], [sample_energy.max() - mean_energy]]
            p_err = [[mean_points - sample_points.min()], [sample_points.max() - mean_points]]
            accel_max = 100
            skidpad_max = 75
            autocross_max = 125
            endurance_max = 275
            efficiency_max = 100
            total_max = 100 + 75 + 125 + 275 + 100
            accel_weight = 1.5 * accel_max / total_max
            skidpad_weight = 1.5 * skidpad_max / total_max
            autocross_weight = autocross_max / total_max
            endurance_weight = endurance_max / total_max
            efficiency_weight = efficiency_max / total_max
            
            if i >= 1:
                # Clamped to >=0: these bars only show an actual loss (red, accel+skidpad
                # got worse vs. the previous pack) or actual gain (green, endurance got
                # better) -- matplotlib's errorbar rejects negative yerr, and a sign flip
                # (e.g. accel+skidpad improved) isn't a "loss" so it draws as a zero-length
                # bar rather than going negative.
                accel_err = max(0.0, prev_accel - mean_accel)
                skidpad_err = max(0.0, prev_skidpad - mean_skidpad)
                endur_err = max(0.0, mean_endur - prev_endur)
                # Recomputed every iteration (not just i == 1) so pack 3+ doesn't reuse a
                # stale delta from the first comparison.
                accel_err_weight = max(0.0, prev_accel_weight - mean_accel / accel_max) * accel_weight
                skidpad_err_weight = max(0.0, prev_skidpad_weight - mean_skidpad / skidpad_max) * skidpad_weight
                endur_err_weight = max(0.0, mean_endur / endurance_max - prev_endur_weight) * endurance_weight

                labels = {"label": "Lower Error"} if i == 1 else {}
                ax6.errorbar(mean_energy, mean_points,
                             yerr=[[accel_err + skidpad_err], [0.0]],
                             fmt="o", ecolor="red", capsize=4, **labels)
                labels = {"label": "Upper Error"} if i == 1 else {}
                ax6.errorbar(mean_energy, mean_points,
                             yerr=[[0.0], [endur_err]],
                             fmt="o", ecolor="green", capsize=4, **labels)
                ax6.annotate(f"{design_e:.0f} Wh", (mean_energy, mean_points),
                             textcoords="offset points", xytext=(6, 6), fontsize=7)

                labels = {"label": "Lower Error"} if i == 1 else {}
                ax7.errorbar(mean_energy, mean_points / total_max,
                             yerr=[[accel_err_weight + skidpad_err_weight], [0.0]],
                             fmt="o", ecolor="red", capsize=4, **labels)
                labels = {"label": "Upper Error"} if i == 1 else {}
                ax7.errorbar(mean_energy, mean_points / total_max,
                             yerr=[[0.0], [endur_err_weight]],
                             fmt="o", ecolor="green", capsize=4, **labels)
                ax7.annotate(f"{design_e:.0f} Wh", (mean_energy, mean_points),
                             textcoords="offset points", xytext=(6, 6), fontsize=7)

            prev_accel = mean_accel
            prev_endur = mean_endur
            prev_skidpad = mean_skidpad
            prev_accel_weight = mean_accel / accel_max
            prev_skidpad_weight = mean_skidpad / skidpad_max
            prev_endur_weight = mean_endur / endurance_max

            ax4.errorbar(mean_energy, mean_points, xerr=e_err, yerr=p_err,
                         fmt="o", capsize=4, label=label)
            ax4.annotate(f"{design_e:.0f} Wh", (mean_energy, mean_points),
                         textcoords="offset points", xytext=(6, 6), fontsize=7)
            fig4_records.append({
                "label": label, "design_e": design_e,
                "masses": np.array(masses), "points": sample_points,
            })
        except Exception as exc:
            print(f"[pack_mass_error_bars] mass error bars failed for {label} ({exc}); "
                  "falling back to a single point, no error bars")
            single = cp.evaluate_pack((series, parallel), int(round(base_mass)))
            if np.isnan(single["total_points"]):
                print(f"[pack_mass_error_bars] {label} failed outright, skipping")
                continue
            ax4.scatter(single[energy_col] * 1000.0, single["total_points"], s=40, label=label)
            ax4.annotate(f"{design_e:.0f} Wh", (single[energy_col] * 1000.0, single["total_points"]),
                         textcoords="offset points", xytext=(6, 6), fontsize=7)

    ax4.set_xlabel(f"{'Endurance energy consumed' if cp.ENERGY_METRIC == 'final' else 'Nominal pack energy'} (Wh)")
    ax4.set_ylabel("Total points")
    ax4.set_title(
        "Competition Points vs. Energy Across Representative Pack Configurations\n"
        f"(error bars: {cp.FIG4_NON_BATTERY_MASS_KG} kg + cell mass, "
        f"+{cp.FIG4_MASS_OFFSETS_KG[1]} to +{cp.FIG4_MASS_OFFSETS_KG[-1]} kg; point labels = nameplate energy)"
    )
    ax4.legend(fontsize=8)

    ax6.set_xlabel(f"{'Endurance energy consumed' if cp.ENERGY_METRIC == 'final' else 'Nominal pack energy'} (Wh)")
    ax6.set_ylabel("Total points")
    ax6.set_title("Points vs. Energy: Change from Previous Pack\nRed = accel+skidpad loss, Green = endurance gain")
    ax6.legend(fontsize=8)

    ax7.set_xlabel(f"{'Endurance energy consumed' if cp.ENERGY_METRIC == 'final' else 'Nominal pack energy'} (Wh)")
    ax7.set_ylabel("Total points (fraction of max)")
    ax7.set_title("Normalized Points vs. Energy: Change from Previous Pack\nRed = accel+skidpad loss, Green = endurance gain, 1.5x Accel/Skidpad Normalization")
    ax7.legend(fontsize=8)
    
    # one line per fixed pack design, so mass/points variance can be compared side by side
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
        print("[pack_mass_error_bars] no pack had a full mass sweep to show mass/points variance for; skipping")


    plt.show()


if __name__ == "__main__":
    main()

"""Reproduces point_mass_sim_mass_endur.m + test_graph_energy.m.

For a given mass and endurance energy budget, `PowerEnergySearch` steps the regulatory
power limit down from the motor's hard limit until both the endurance peak electrical
power and total energy consumption fall within target bounds, then scores the result.

In the original script, the full mass x energy grid sweep (the `arrayfun` calls building
`eScores_208_mixed_ext2` etc., consumed by test_graph_energy.m's heatmaps) was commented
out day-to-day in favor of a single quick-check call -- it is slow, since every grid cell
involves its own iterative power search, each step of which is a full accel/skidpad/
endurance simulation. That single call is what runs by default here too; pass `--grid`
to run the full sweep and reproduce the test_graph_energy.m plots (expect a long runtime).
"""
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lap_sim import (
    ACCEL_EVENT, EMRAX_208, ENDURANCE_EVENT, EV25, FSAE_EV, SKIDPAD_EVENT,
    CompetitionEvaluator, CompetitionScorer, PowerEnergySearch, grid_sweep, plotting,
)
from lap_sim.vehicle import Aero, Car, Drivetrain, HighVoltageSystem

DX = 0.01  # matches point_mass_sim_mass_endur.m's lapsim resolution


def build_ev26b() -> Car:
    """The EV26B spec specific to point_mass_sim_mass_endur.m (same numbers as the
    gear-ratio study's EV26B)."""
    return Car(
        name="EV26B (mass_energy_power_search)",
        mass=220 + 68,
        cg=EV25.cg,
        aero=Aero(cda=1.7, cla=3.2),
        tire=EV25.tire,
        drivetrain=Drivetrain(motor=EMRAX_208, ratio=4.9, efficiency=0.96, count=1),
        hv=HighVoltageSystem(vmax=300, vnom=260),
        l=1.530,
    )


def run_grid_sweep(search: PowerEnergySearch, baseline: Car, masses, energies_wh):
    def evaluate(energy_target_wh, mass):
        result = search.find(baseline, FSAE_EV, mass=mass, energy_target_wh=energy_target_wh)
        return {
            "score": result["score"],
            "avg_power_w": result["avg_electric_w"],
            "peak_power_w": result["peak_electric_w"],
            "power_limit": result["power_limit"],
        }

    return grid_sweep({"energy_target_wh": energies_wh, "mass": masses}, evaluate)


def main():
    baseline = build_ev26b()
    evaluator = CompetitionEvaluator(ACCEL_EVENT, SKIDPAD_EVENT, ENDURANCE_EVENT, CompetitionScorer(), dx=DX)
    search = PowerEnergySearch(evaluator)

    # The single call that actually runs by default in the source script.
    quick = search.find(baseline, FSAE_EV, mass=baseline.mass, energy_target_wh=5.5e3 + 0.55e3)
    print(
        f"Quick check @ baseline mass, 6.05 kWh budget: power limit = {quick['power_limit'] / 1e3:.1f} kW, "
        f"score = {quick['score']:.1f}, endurance time = {quick['endurance_time']:.1f} s, "
        f"avg power = {quick['avg_electric_w'] / 1e3:.2f} kW"
    )

    if "--grid" not in sys.argv:
        print("Pass --grid to run the full mass x energy sweep (slow) and plot the heatmaps.")
        return

    masses = baseline.mass + np.arange(-40.0, 101.0, 10.0)
    energies_wh = np.arange(3600.0, 9001.0, 200.0)
    df = run_grid_sweep(search, baseline, masses, energies_wh)

    score_pivot = df.pivot(index="mass", columns="energy_target_wh", values="score")
    peak_pivot = df.pivot(index="mass", columns="energy_target_wh", values="peak_power_w")
    avg_pivot = df.pivot(index="mass", columns="energy_target_wh", values="avg_power_w")
    x_vals, y_vals = score_pivot.columns.values, score_pivot.index.values

    fig1_ax = plt.figure().gca()
    plotting.plot_heatmap(
        x_vals, y_vals, score_pivot.values, "Energy (Wh)", "Mass (kg)", "Points",
        "Mass and Energy to Points Comparison for Single Emrax 208", ax=fig1_ax,
    )
    plotting.plot_energy_density_lines(
        fig1_ax, x_vals, np.arange(baseline.mass - 70, baseline.mass + 51, 10),
        [149.9, 128.379, 146.6, 149.26],
    )

    plotting.plot_heatmap(
        x_vals, y_vals, peak_pivot.values / 1e3, "Energy (Wh)", "Mass (kg)", "Peak Power (kW)",
        "Mass and Energy to Peak Power Comparison for Single Emrax 208",
    )
    plotting.plot_heatmap(
        x_vals, y_vals, avg_pivot.values / 1e3, "Energy (Wh)", "Mass (kg)", "Average Power (kW)",
        "Mass and Energy to Avg Power Comparison for Single Emrax 208",
    )
    plotting.plot_surface(
        x_vals, y_vals, score_pivot.values, "Energy (Wh)", "Mass (kg)", "Points",
        "Mass and Energy to Points Comparison for Single Emrax 208",
    )

    plt.show()


if __name__ == "__main__":
    main()

"""Reproduces point_mass_sim_mass_powerlim208.m + test_graph.m: a grid sweep over
power limit x mass for a single-Emrax-208 car, scored with the accel/skidpad/endurance
events (old-style scoring, without the efficiency term), plotted as heatmaps/surfaces.

This is a genuinely large sweep (21 power limits x 21 masses x 3 events each, with the
endurance event alone at dx=0.005 m over a 1000 m lap = ~200k simulation points) -- it
was slow in the original MATLAB too. Expect this to take a while to run in full.
"""
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lap_sim import (
    ACCEL_EVENT, EMRAX_208, ENDURANCE_EVENT, EV25, FSAE_EV, SKIDPAD_EVENT,
    CompetitionEvaluator, CompetitionScorer, grid_sweep, plotting,
)
from lap_sim.vehicle import Aero, Car, Drivetrain, HighVoltageSystem

DX = 0.005  # matches point_mass_sim_mass_powerlim208.m's lapsim resolution


def build_ev26b() -> Car:
    """The EV26B spec specific to point_mass_sim_mass_powerlim208.m."""
    return Car(
        name="EV26B (mass_power_grid_sweep)",
        mass=220 + 68,
        cg=EV25.cg,
        aero=Aero(cda=0.2288, cla=0.51),
        tire=EV25.tire,
        drivetrain=Drivetrain(motor=EMRAX_208, ratio=4.9, efficiency=0.96, count=1),
        hv=HighVoltageSystem(vmax=300, vnom=260),
        l=1.530,
    )


def main():
    baseline = build_ev26b()
    evaluator = CompetitionEvaluator(ACCEL_EVENT, SKIDPAD_EVENT, ENDURANCE_EVENT, CompetitionScorer(), dx=DX)

    power_limits = FSAE_EV.power_limit + np.arange(-69500.0, -18500.0, 2500.0)
    masses = baseline.mass + np.arange(-40.0, 41.0, 4.0)

    def evaluate(power_limit, mass):
        car = baseline.replace(mass=mass)
        regulations = FSAE_EV.replace(power_limit=power_limit)
        result = evaluator.evaluate(car, regulations, include_efficiency=False)
        return {
            "score": result["score"],
            "avg_power_w": result["avg_electric_w"],
            "peak_power_w": result["peak_electric_w"],
        }

    df = grid_sweep({"power_limit": power_limits, "mass": masses}, evaluate)

    score_pivot = df.pivot(index="mass", columns="power_limit", values="score")
    peak_pivot = df.pivot(index="mass", columns="power_limit", values="peak_power_w")
    avg_pivot = df.pivot(index="mass", columns="power_limit", values="avg_power_w")
    x_vals, y_vals = score_pivot.columns.values, score_pivot.index.values

    plotting.plot_heatmap(
        x_vals, y_vals, score_pivot.values, "Power Limit (W)", "Mass (kg)", "Points",
        "Mass & Power Limit to Points (Single Emrax 208)",
    )
    plotting.plot_heatmap(
        x_vals, y_vals, peak_pivot.values / 1e3, "Power Limit (W)", "Mass (kg)",
        "Peak Power (kW)", "Mass & Power Limit to Peak Power (Single Emrax 208)",
    )
    plotting.plot_heatmap(
        x_vals, y_vals, avg_pivot.values / 1e3, "Power Limit (W)", "Mass (kg)",
        "Average Power (kW)", "Mass & Power Limit to Avg Power (Single Emrax 208)",
    )
    plotting.plot_surface(
        x_vals, y_vals, score_pivot.values, "Power Limit (W)", "Mass (kg)", "Points",
        "Mass & Power Limit to Points (Single Emrax 208)",
    )

    plt.show()


if __name__ == "__main__":
    main()

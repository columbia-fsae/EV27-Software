"""Reproduces point_mass_sim_gear_ratio.m: sweep final drive ratio and plot its effect
on the FSAE acceleration event time.
"""
import sys
from dataclasses import replace
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lap_sim import ACCEL_EVENT, EMRAX_208, EV25, FSAE_EV, LapSimulator
from lap_sim.vehicle import Aero, Car, Drivetrain, HighVoltageSystem

DX = 0.01  # matches point_mass_sim_gear_ratio.m's lapsim resolution


def build_ev26b() -> Car:
    """The EV26B spec specific to point_mass_sim_gear_ratio.m."""
    return Car(
        name="EV26B (gear_ratio_sweep)",
        mass=220 + 68,
        cg=EV25.cg,
        aero=Aero(cda=1.7, cla=3.2),
        tires=EV25.tires,
        drivetrain=Drivetrain(motor=EMRAX_208, ratio=4.9, efficiency=0.96, count=1),
        hv=HighVoltageSystem(vmax=300, vnom=260),
        l=1.530,
    )


def accel_time_for_ratio(car: Car, ratio: float) -> float:
    car = car.replace(drivetrain=replace(car.drivetrain, ratio=ratio))
    result = LapSimulator(FSAE_EV, ACCEL_EVENT, car, dx=DX).run()
    return result.split_time(1, 2)


def main():
    baseline = build_ev26b()
    ratios = np.arange(3.0, 6.0 + 1e-9, 0.1)
    times = np.array([accel_time_for_ratio(baseline, r) for r in ratios])

    best_idx = int(np.argmin(times))
    print(f"Best ratio: {ratios[best_idx]:.1f} -> accel time {times[best_idx]:.3f} s")

    fig, ax = plt.subplots()
    ax.plot(ratios, times)
    ax.set_xlabel("Final drive ratio")
    ax.set_ylabel("Accel event time (s)")
    ax.set_title("Accel time vs. final drive ratio")
    plt.show()


if __name__ == "__main__":
    main()

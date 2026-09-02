"""Runs the skidpad event for a range of car masses and plots the resulting vertical
tire load (Fz = mass*g + aero downforce) against speed for each mass, alongside skidpad
lap time vs. mass.
"""
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lap_sim import EMRAX_208, EV27, FSAE_EV, SKIDPAD_EVENT, LapSimulator
from lap_sim.vehicle import Aero, Car, Drivetrain, HighVoltageSystem

DX = 0.005  # matches run_events.py's skidpad resolution
MASS_VALUES = np.arange(200, 400, 20)  # kg, whole car incl. driver
GRAVITY = 9.806  # m/s^2, matches lap_sim.dynamics.GRAVITY


def build_car(mass: float) -> Car:
    return Car(
        name=f"EV27 ({mass:.0f} kg)",
        mass=mass,
        cg=np.array([742.44, 0.0, 248.52]),
        aero=Aero(cda=1.7, cla=3.05),
        tire=EV27.tire,
        drivetrain=Drivetrain(motor=EMRAX_208, ratio=4.3, efficiency=0.96, count=1),
        hv=HighVoltageSystem(vmax=255, vnom=216),
        l=1.530,
        battery=None,  # not needed for the skidpad's steady-state cornering speed
    )


def main():
    fig, ax = plt.subplots(figsize=(8, 6))
    lap_times = []

    for mass in MASS_VALUES:
        car = build_car(mass)
        skidpad = LapSimulator(FSAE_EV, SKIDPAD_EVENT, car, dx=DX).run()
        v = skidpad.vv
        fz = GRAVITY * mass + 0.5 * SKIDPAD_EVENT.air_density * car.aero.cla * v * v
        # Matches run_events.py's official skidpad time: one steady-state circuit
        # (segments 2-3), excluding the entry/exit straights.
        lap_times.append(skidpad.split_time(2, 3))

        order = np.argsort(v)
        ax.plot(v[order] * 3.6, fz[order], label=f"{mass:.0f} kg")

    ax.set_xlabel("Speed (km/h)")
    ax.set_ylabel("Fz (N)")
    ax.set_title("Skidpad Vertical Tire Load vs. Speed for Different Car Masses")
    ax.legend(title="Car mass")
    fig.tight_layout()

    fig2, ax2 = plt.subplots(figsize=(8, 6))
    ax2.plot(MASS_VALUES, lap_times, "o-")
    for mass, lap_time in zip(MASS_VALUES, lap_times):
        ax2.annotate(f"{lap_time:.3f}s", (mass, lap_time),
                     textcoords="offset points", xytext=(0, 6), fontsize=7, ha="center")
    ax2.set_xlabel("Car mass (kg)")
    ax2.set_ylabel("Skidpad lap time (s)")
    ax2.set_title("Skidpad Lap Time vs. Car Mass")
    fig2.tight_layout()

    plt.show()


if __name__ == "__main__":
    main()

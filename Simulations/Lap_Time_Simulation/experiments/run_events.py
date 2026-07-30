"""Reproduces point_mass_sim_final.m: accel, skidpad, Icahn loop, and endurance runs for
a single car, with the original script's plots and FSAE points calculation.
"""
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lap_sim import (
    ACCEL_EVENT, EMRAX_208, ENDURANCE_EVENT, EV25, FSAE_EV, ICAHN_LOOP, SKIDPAD_EVENT,
    CompetitionScorer, LapSimulator,
)
from lap_sim import plotting
from lap_sim.vehicle import Aero, Car, Drivetrain, HighVoltageSystem

DX = 0.005  # matches point_mass_sim_final.m's lapsim resolution


def build_ev26b() -> Car:
    """The EV26B spec specific to point_mass_sim_final.m (differs from the other
    scripts' EV26B in mass, aero, gear ratio, HV window, and motor power cap)."""
    motor = EMRAX_208.with_power_limit(41.5e3)
    return Car(
        name="EV26B (run_events)",
        mass=207 + 60,
        cg=np.array([742.44, 0.0, 248.52]),
        aero=Aero(cda=1.7, cla=3.05),
        tires=EV25.tires,
        drivetrain=Drivetrain(motor=motor, ratio=4.3, efficiency=0.96, count=1),
        hv=HighVoltageSystem(vmax=255, vnom=216),
        l=1.530,
    )


def main():
    ev26b = build_ev26b()
    scorer = CompetitionScorer()

    accel = LapSimulator(FSAE_EV, ACCEL_EVENT, ev26b, dx=DX).run()
    print(f"[Accel]    peak electric power = {accel.stats.pelectric.max() / 1e3:.1f} kW, "
          f"top speed = {accel.vv.max() * 3.6:.1f} km/h")
    plotting.plot_lap_overview([accel])
    accel_time = accel.split_time(1, 2)
    print(f"[Accel]    time = {accel_time:.3f} s")

    skidpad = LapSimulator(FSAE_EV, SKIDPAD_EVENT, ev26b, dx=DX).run()
    plotting.plot_lap_overview([skidpad])
    skidpad_time = skidpad.split_time(2, 3)
    print(f"[Skidpad]  time = {skidpad_time:.3f} s")

    icahn = LapSimulator(FSAE_EV, ICAHN_LOOP, ev26b, dx=DX).run()
    plotting.plot_lap_overview([icahn])
    icahn_time = icahn.lap_time
    print(f"[Icahn]    lap time = {icahn_time:.3f} s")

    # The original script drops the power limit to 5 kW right before the endurance run
    # (unusually low relative to the 80 kW regulation) -- kept verbatim here.
    endurance_regs = FSAE_EV.replace(power_limit=5e3)
    endur = LapSimulator(endurance_regs, ENDURANCE_EVENT, ev26b, dx=DX).run()
    print(f"[Endur]    peak electric power = {endur.stats.pelectric.max() / 1e3:.1f} kW, "
          f"top speed = {endur.vv.max() * 3.6:.1f} km/h")
    plotting.plot_lap_overview([endur])
    endur_time = endur.lap_time * 22

    fig, (ax_v, ax_a) = plt.subplots(2, 1, figsize=(7, 6))
    ax_v.plot(endur.vv)
    ax_v.set_ylabel("Speed (m/s)")
    ax_a.plot(endur.stats.ax)
    ax_a.set_ylabel("ax (m/s^2)")
    fig.suptitle("Endurance speed & longitudinal acceleration trace")

    fig_gg, ax_gg = plt.subplots()
    ax_gg.plot(endur.stats.ax, endur.stats.ay, "*")
    ax_gg.set_xlabel("ax (m/s^2)")
    ax_gg.set_ylabel("ay (m/s^2)")
    ax_gg.set_title("Endurance G-G diagram")

    avg_power = endur.stats.avg_electric
    energy_wh = avg_power * endur_time / 3600.0
    print(f"[Endur]    22-lap time = {endur_time:.1f} s, avg electric power = "
          f"{avg_power / 1e3:.2f} kW, energy = {energy_wh:.1f} Wh")

    score = scorer.score(accel_time, skidpad_time, endur_time * 0.8 / 22, endur_time, energy_wh)
    print(
        f"[Score]    total = {score.total:.1f}  "
        f"(accel {score.accel:.1f}, skidpad {score.skidpad:.1f}, "
        f"autocross {score.autocross:.1f}, endurance {score.endurance:.1f}, "
        f"efficiency {score.efficiency:.1f})"
    )

    plt.show()


if __name__ == "__main__":
    main()

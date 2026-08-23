"""Validates lap_sim's Cell/Battery equivalent-circuit model against logged pack
telemetry (TDMS files from the EnergyMeter/ISASensor DAQ).

Each TDMS log has a "Data" group (Voltage, Current, Energy, GLV, Violation,
TeamSignal1-4 at 100 Hz) and a "Temperature" group (Temperature1-5 at 1 Hz, one
channel per sensor). This script feeds the logged pack current and timestep into
`lap_sim.Battery.step`, then plots the model's predicted terminal voltage/SOC/cell
temperature against what was actually measured.

Usage:
    python experiments/tdms_battery_validation.py path/to/log1.tdms path/to/log2.tdms
    python experiments/tdms_battery_validation.py log.tdms --series 105 --parallel 4
"""
import argparse
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from nptdms import TdmsFile

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lap_sim.battery import Battery

# Matches vehicle.py's _BATTERY_EV26_27 pack config; override with --series/--parallel
# if the logged car used a different pack.
DEFAULT_SERIES = 105
DEFAULT_PARALLEL = 4
DEFAULT_CELL_TYPE = "ampace_jp50"


def load_tdms(path: Path) -> dict:
    """Pull the Data-group channels (as float arrays) and the Temperature-group
    channels (averaged across sensors) out of one TDMS log."""
    tdms = TdmsFile.read(path)
    data = tdms["Data"]

    voltage = data["Voltage"][:].astype(float)
    current = data["Current"][:].astype(float)
    energy = data["Energy"][:].astype(float)
    dt = float(data["Voltage"].properties["wf_increment"])
    t = np.arange(len(voltage)) * dt

    temps = None
    if "Temperature" in tdms:
        temp_group = tdms["Temperature"]
        temp_channels = list(temp_group.channels())
        if temp_channels:
            temp_dt = float(temp_channels[0].properties["wf_increment"])
            n_temp = len(temp_channels[0][:])
            t_temp = np.arange(n_temp) * temp_dt
            temp_avg = np.mean([c[:].astype(float) for c in temp_channels], axis=0)
            temps = (t_temp, temp_avg)

    return {"t": t, "dt": dt, "voltage": voltage, "current": current, "energy": energy, "temps": temps}


def estimate_initial_soc(battery: Battery, voltage: np.ndarray, current: np.ndarray) -> float:
    """Back out a starting SOC via the cell's OCV(SOC) curve.

    Logs start with a precharge ramp (voltage climbing from near 0 while current
    sits near 0), so t=0 isn't a usable OCV sample. Instead, use the settled
    voltage right before the pack starts drawing real current.
    """
    idle = np.argmax(np.abs(current) > 5.0) if np.any(np.abs(current) > 5.0) else 0
    window = voltage[max(0, idle - 50):max(1, idle)]
    pack_v0 = float(np.median(window)) if len(window) else voltage[0]

    cell_v0 = pack_v0 / battery._series
    ocv_vals = battery._soc_ocv[:, 1]
    soc_vals = battery._soc_ocv[:, 0]
    order = np.argsort(ocv_vals)
    soc0 = float(np.interp(cell_v0, ocv_vals[order], soc_vals[order]))
    return np.clip(soc0, 0.0, 1.0)


def simulate(battery: Battery, current_measured: np.ndarray, dt: float) -> dict:
    """Step the battery model through the whole log.

    TDMS current is signed negative-for-discharge; Cell/Battery's convention is
    positive-i-depletes-SOC, so the sign is flipped before calling `battery.step`.
    """
    n = len(current_measured)
    v_model = np.empty(n)
    soc_model = np.empty(n)
    temp_model = np.empty(n)
    ocv_model = np.empty(n)

    for k in range(n):
        i_model = -current_measured[k]
        battery.step(i_model, dt)
        v_model[k] = battery.voltage
        soc_model[k] = battery.soc
        temp_model[k] = battery.cell_T
        ocv_model[k] = battery._ocv * battery._series

    return {"voltage": v_model, "soc": soc_model, "temp": temp_model, "ocv": ocv_model}


def plot_validation(path: Path, log: dict, sim: dict, series: int) -> plt.Figure:
    t = log["t"]
    v_err = log["voltage"] - sim["voltage"]
    rmse = np.sqrt(np.mean(v_err**2))

    fig, axes = plt.subplots(3, 2, figsize=(13, 11))
    fig.suptitle(f"{path.name}  (voltage RMSE = {rmse:.2f} V)")

    ax = axes[0, 0]
    ax.plot(t, log["voltage"], label="Measured", linewidth=1)
    ax.plot(t, sim["voltage"], label="Model", linewidth=1, linestyle="--")
    ax.set_ylabel("Pack Voltage (V)")
    ax.legend()

    ax = axes[0, 1]
    ax.plot(t, v_err, color="tab:red", linewidth=0.8)
    ax.axhline(0, color="k", linewidth=0.5)
    ax.set_ylabel("Voltage Error, meas-model (V)")

    ax = axes[1, 0]
    ax.plot(t, log["current"], color="tab:orange", linewidth=1)
    ax.set_ylabel("Measured Current (A)")

    ax = axes[1, 1]
    ax.plot(t, sim["soc"] * 100, color="tab:green", linewidth=1)
    ax.set_ylabel("Modeled SOC (%)")

    ax = axes[2, 0]
    ax.plot(t, sim["temp"], label="Model (cell)", color="tab:purple", linewidth=1)
    if log["temps"] is not None:
        t_temp, temp_avg = log["temps"]
        ax.plot(t_temp, temp_avg, label="Measured (pack avg)", color="tab:brown", linewidth=1)
    ax.set_ylabel("Temperature (C)")
    ax.set_xlabel("Time (s)")
    ax.legend()

    ax = axes[2, 1]
    ax.plot(t, log["energy"], label="Measured", linewidth=1)
    model_energy = np.concatenate([[0.0], np.cumsum(sim["voltage"][:-1] * log["current"][:-1]) * log["dt"] / 3600])
    ax.plot(t, log["energy"][0] + model_energy, label="Model", linewidth=1, linestyle="--")
    ax.set_ylabel("Energy (Wh)")
    ax.set_xlabel("Time (s)")
    ax.legend()

    for ax in axes[:2].flat:
        ax.set_xlabel("Time (s)")
    fig.tight_layout()
    return fig


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("tdms_files", nargs="+", type=Path, help="One or more TDMS log files")
    parser.add_argument("--series", type=int, default=DEFAULT_SERIES)
    parser.add_argument("--parallel", type=int, default=DEFAULT_PARALLEL)
    parser.add_argument("--cell-type", default=DEFAULT_CELL_TYPE)
    parser.add_argument("--initial-temp", type=float, default=25.0, help="Starting cell temp (C)")
    parser.add_argument("--initial-soc", type=float, default=None,
                         help="Starting SOC (0-1); default estimates it from the first logged voltage")
    parser.add_argument("--save-dir", type=Path, default=None, help="Save PNGs here instead of showing plots")
    args = parser.parse_args()

    for path in args.tdms_files:
        print(f"[tdms_battery_validation] loading {path.name} ...")
        log = load_tdms(path)
        n = len(log["voltage"])
        print(f"[tdms_battery_validation] {n} samples at dt={log['dt']}s "
              f"({n * log['dt'] / 60:.1f} min); running battery model ...")

        battery = Battery(series=args.series, parallel=args.parallel, cell_type=args.cell_type)
        battery._T = args.initial_temp
        soc0 = args.initial_soc if args.initial_soc is not None else estimate_initial_soc(battery, log["voltage"], log["current"])
        battery._x[0] = soc0
        battery._soc = soc0
        print(f"[tdms_battery_validation] initial SOC = {soc0 * 100:.1f}%, initial temp = {args.initial_temp} C")

        sim = simulate(battery, log["current"], log["dt"])
        rmse = np.sqrt(np.mean((log["voltage"] - sim["voltage"]) ** 2))
        print(f"[tdms_battery_validation] done: voltage RMSE = {rmse:.2f} V, "
              f"final SOC = {sim['soc'][-1] * 100:.1f}%, final cell temp = {sim['temp'][-1]:.1f} C")

        fig = plot_validation(path, log, sim, args.series)
        if args.save_dir is not None:
            args.save_dir.mkdir(parents=True, exist_ok=True)
            out_path = args.save_dir / f"{path.stem}_battery_validation.png"
            fig.savefig(out_path, dpi=150)
            print(f"[tdms_battery_validation] saved {out_path}")

    if args.save_dir is None:
        plt.show()


if __name__ == "__main__":
    main()

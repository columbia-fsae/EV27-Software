"""Software-in-the-loop runner for the real embedded SOC estimator.

Compiles (if needed) and runs the unmodified kalman_soc.c / battery_model.c (see c_src/)
against a CSV of (time_ms, voltage, current[, temperature]) using the same
Init/SetRCLookupTables/Update call sequence the ADBMS6830 firmware uses, then plots the
resulting SOC estimate.

Usage:
    python run_soc_sil.py [input.csv] [-o output.csv] [--rebuild]

With no arguments, runs against the bundled synthetic example
(generate it first with `python generate_example_data.py`).
"""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

ROOT = Path(__file__).resolve().parent
C_SRC = ROOT / "c_src"
HARNESS_SRC = ROOT / "harness" / "sil_harness.c"
BIN_DIR = ROOT / "bin"
EXECUTABLE = BIN_DIR / ("soc_sil.exe" if sys.platform == "win32" else "soc_sil")


def find_compiler() -> str:
    for candidate in ("cc", "gcc", "clang"):
        if shutil.which(candidate):
            return candidate
    raise RuntimeError(
        "No C compiler found (looked for cc/gcc/clang on PATH).\n"
        "In WSL/Linux: sudo apt install build-essential\n"
        "On native Windows: install MinGW-w64, or just run this from WSL."
    )


def build(force: bool = False) -> Path:
    BIN_DIR.mkdir(exist_ok=True)
    sources = [C_SRC / "kalman_soc.c", C_SRC / "battery_model.c", HARNESS_SRC]
    # Everything that can affect the build, including headers -- kalman_soc_config.h in
    # particular is where the filter's tuning constants (Q_SOC, R_VOLTAGE, INITIAL_R0,
    # etc.) live, and a stale binary must not be reused just because no .c file changed.
    all_deps = sorted(C_SRC.glob("*.h")) + sorted(C_SRC.glob("*.c")) + [HARNESS_SRC]

    if not force and EXECUTABLE.exists():
        newest_dep = max(s.stat().st_mtime for s in all_deps)
        if EXECUTABLE.stat().st_mtime >= newest_dep:
            return EXECUTABLE  # up to date

    compiler = find_compiler()
    cmd = [
        compiler, "-O2", "-std=c11", "-Wall", "-Wextra",
        f"-I{C_SRC}",
        *[str(s) for s in sources],
        "-o", str(EXECUTABLE),
        "-lm",
    ]
    print("Building:", " ".join(cmd))
    subprocess.run(cmd, check=True)
    return EXECUTABLE


def run_sil(input_csv: Path, output_csv: Path, force_rebuild: bool = False) -> pd.DataFrame:
    exe = build(force=force_rebuild)
    subprocess.run([str(exe), str(input_csv), str(output_csv)], check=True)
    return pd.read_csv(output_csv)


def add_diagnostics(df: pd.DataFrame) -> pd.DataFrame:
    """Reconstructs the filter's internal predicted terminal voltage and the resulting
    innovation (measured - predicted), purely from the output CSV's existing columns --
    no C changes needed. This is an approximation: it uses the *post-update* soc/v_ct/
    v_dif/r0 for a given row (kalman_soc.c doesn't expose the pre-update predicted state),
    so it lags the filter's true internal v[0] by one step, but it's close enough to see
    where the model and the measurement diverge, e.g. right as current drops to zero.

    v[0] (in kalman_soc.c) = OCV(soc) - v_ct - v_dif - r0*i
    """
    df = df.copy()
    ocv_from_soc = df["voltage_ocv"]  # BatteryModel_GetOcv(soc), already computed post-update
    df["predicted_terminal_v"] = ocv_from_soc - df["v_ct"] - df["v_dif"] - df["r0"] * df["input_current"]
    df["voltage_innovation_mV"] = (df["input_voltage"] - df["predicted_terminal_v"]) * 1000.0
    return df


def plot_results(df: pd.DataFrame, title: str):
    df = add_diagnostics(df)
    t_s = df["time_ms"] / 1000.0

    fig, axes = plt.subplots(4, 2, figsize=(12, 13), sharex=True)

    ax = axes[0, 0]
    ax.plot(t_s, df["soc_percent"])
    ax.set_ylabel("SOC (%)")
    ax.set_title("Estimated State of Charge")

    ax = axes[0, 1]
    ax.plot(t_s, df["input_voltage"], label="Measured terminal V")
    ax.plot(t_s, df["voltage_ocv"], label="Estimated OCV")
    ax.set_ylabel("Voltage (V)")
    ax.set_title("Terminal Voltage vs. Estimated OCV")
    ax.legend(fontsize=8)

    ax = axes[1, 0]
    ax.plot(t_s, df["input_current"], color="tab:orange")
    ax.set_ylabel("Current (A)")
    ax.set_title("Input Current (+ = discharge)")

    ax = axes[1, 1]
    l1, = ax.plot(t_s, df["r0"] * 1000.0, color="tab:blue", label="R0 (mOhm)")
    ax.set_ylabel("R0 (mOhm)")
    ax_soh = ax.twinx()
   # l2, = ax_soh.plot(t_s, df["soh_percent"], color="tab:green", label="SOH (%)")
   # ax_soh.set_ylabel("SOH (%)")
   # ax.set_title("Estimated R0 & State of Health")
    ax.legend(handles=[l1], fontsize=8)

    ax = axes[2, 0]
    ax.plot(t_s, df["v_ct"], label="v_ct")
    ax.plot(t_s, df["v_dif"], label="v_dif")
    ax.set_ylabel("Voltage (V)")
    ax.set_xlabel("Time (s)")
    ax.set_title("RC Branch States")
    ax.legend(fontsize=8)

    ax = axes[2, 1]
    ax.plot(t_s, df["uncertainty"])
    ax.set_ylabel("uncertainty (sqrt(P_vct)))")
    ax.set_title("Filter Uncertainty")

    ax = axes[3, 0]
    ax.plot(t_s, df["input_voltage"], label="Measured V", linewidth=1)
    ax.plot(t_s, df["predicted_terminal_v"], label="Predicted V (OCV-v_ct-v_dif-R0*I)", linewidth=1)
    ax.set_ylabel("Voltage (V)")
    ax.set_xlabel("Time (s)")
    ax.set_title("Measured vs. Reconstructed Predicted Terminal Voltage")
    ax.legend(fontsize=8)

    ax = axes[3, 1]
    ax.axhline(0, color="gray", linewidth=0.8)
    ax.plot(t_s, df["voltage_innovation_mV"], color="tab:red", linewidth=1)
    ax.set_ylabel("Innovation (mV)")
    ax.set_xlabel("Time (s)")
    ax.set_title("Measured - Predicted (watch this at current-stop transitions)")
    '''
    ax = axes[4, 0]
    ax.axhline(0, color="gray", linewidth=0.8)
    ax.plot(t_s, df["capacity_ah"], color="tab:red", linewidth=1)
    ax.set_ylabel("Capacity (ah)")
    ax.set_xlabel("Time (s)")
    ax.set_title("Real-time Capacity Estimation")

    ax = axes[4, 1]
    bypass_times = t_s[df["bypassed"] == 1]
    ax.vlines(bypass_times, 0, 1, color="tab:red", linewidth=1)
    ax.set_ylim(0, 1.05)
    ax.set_yticks([0, 1])
    ax.set_ylabel("Bypassed")
    ax.set_xlabel("Time (s)")
    ax.set_title(f"Innovation Gate Bypass Events (n={int(df['bypassed'].sum())})")
    '''
    fig.suptitle(title)
    fig.tight_layout()
    return fig


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "input_csv", nargs="?", type=Path,
        default=ROOT / "example_data" / "synthetic_drive_cycle.csv",
        help="CSV with time_ms, voltage, current[, temperature] columns",
    )
    parser.add_argument("-o", "--output", type=Path, default=None,
                         help="Where to write the SOC estimator's output CSV")
    parser.add_argument("--rebuild", action="store_true", help="Force recompiling the SIL harness")
    args = parser.parse_args()

    if not args.input_csv.exists():
        print(f"Input CSV not found: {args.input_csv}")
        if args.input_csv == ROOT / "example_data" / "synthetic_drive_cycle.csv":
            print("Generate it first with: python generate_example_data.py")
        sys.exit(1)

    output_csv = args.output or (ROOT / "example_data" / f"{args.input_csv.stem}_soc_output.csv")
    df = run_sil(args.input_csv, output_csv, force_rebuild=args.rebuild)

    print(df[["soc_percent", "r0", "soh_percent", "uncertainty","capacity_ah"]].describe())

    plot_results(df, f"SOC Estimator SIL Run: {args.input_csv.name}")
    plt.show()


if __name__ == "__main__":
    main()

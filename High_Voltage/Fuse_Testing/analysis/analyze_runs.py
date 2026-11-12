#!/usr/bin/env python3
"""Fuse blow-time analysis.

Reads run CSVs written by pc/fuse_logger.py. The "overview" mode writes one summary CSV with a row per run:

    path          run's folder relative to the deepest folder all runs share (blank if they share one)
    file          run CSV name (includes the run timestamp)
    blow_time_s   time from the first sample with |I| above the threshold to the next sample at or below it
    rms_A         RMS current over that same interval
    error         why the run has no result (blank when it has one)

Only the first above-threshold interval of each run is used. Runs that start above the threshold,
never exceed it, or never fall back below it get empty result cells and the reason in "error".

The "detailed" mode writes, for each run, only the samples of that first interval (every original
column) to <output folder>/<path>/<name>_truncated.csv. Runs with an error get no file.

The "overlay" mode plots that first interval of every run on one current-vs-time plot, each shifted so
it starts at t=0, coloured by the part of the file name before the first underscore (3mm_<timestamp>
-> 3mm, whatever folder it is in). The plot is opened and, if a file is chosen, saved as an image.

The "fuse curve" mode plots blow time vs current on log-log axes, one point per run, grouped by the same
name as overlay, and fits each group with a lumped thermal model of the fuse: I^2*R heating against
cooling proportional to temperature rise, melting at a fixed temperature. In units of A^2 the fuse's
thermal state u follows du/dt = (I(t)^2 - u) / tau and it blows when u reaches I_min^2. For a constant
current that gives the curve

    t = tau * ln(I^2 / (I^2 - I_min^2))

which goes vertical at the minimum fusing current I_min and tends to I^2*t = constant at high current.

The fit uses each run's whole current trace, not one number per run, so runs where the current was
changed part-way are handled. Each run is plotted at its equivalent current: the constant current that
would leave the fuse in the same thermal state at the measured blow time. That is the RMS for runs much
shorter than tau and weights the end of the run more for longer ones. Where it differs from the RMS by
over 5%, a faint hollow marker shows the RMS. A point lies on its curve exactly when the model predicts
that run's blow time. The curve is solid over the measured currents and dashed where it is extrapolated
towards I_min. Groups with fewer than MIN_FIT_POINTS runs, or currents spanning less than MIN_FIT_SPAN,
get no fit and are plotted at their RMS.

The fit is robust (Cauchy loss on log blow time, scale OUTLIER_FACTOR), so a few runs that disagree with
the rest of their group, such as runs stopped by hand, don't drag the curve away from the others. Runs
more than OUTLIER_FACTOR off the fit are drawn as crosses and listed on the console.

An optional blow-time limit discards longer runs in every mode: overview lists them with the reason in
"error", detailed writes no file for them and overlay and fuse curve leave them out.

    python analyze_runs.py                                        # choose mode, runs and output in dialogs
    python analyze_runs.py ..\\logs\\test2 -o out.csv                 # overview, no dialogs
    python analyze_runs.py ..\\logs --mode detailed -o truncated_runs  # detailed, no dialogs
    python analyze_runs.py ..\\logs --mode overlay -o overlay.png      # overlay, no dialogs
    python analyze_runs.py ..\\logs --mode curve -o curves.png         # fuse curve, no dialogs
    python analyze_runs.py --threshold 1.0 --max-blow-time 10      # options also work with the dialogs
"""

import argparse
import csv
import importlib.util
import os
import re
import sys
from pathlib import Path

if importlib.util.find_spec("numpy") is None:
    sys.exit(f'numpy is missing for this Python ({sys.executable}). Install it with:  "{sys.executable}" -m pip install numpy')

import numpy as np  # noqa: E402

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_LOG_DIR = SCRIPT_DIR.parent / "logs"
DEFAULT_THRESHOLD_A = 0.5
OUT_COLUMNS = ["path", "file", "blow_time_s", "rms_A", "error"]
TRUNCATED_SUFFIX = "_truncated"
MIN_FIT_SPAN = 1.2  # fuse curve mode: max/min RMS current a group needs before it gets a fit
MIN_FIT_POINTS = 3  # fuse curve mode: runs a group needs before it gets a fit (the model has 2 parameters)
OUTLIER_FACTOR = 1.5  # fuse curve mode: runs whose blow time is off the fit by more than this factor are outliers


def first_event(t, i, threshold):
    """Returns (start, end, problem) sample indices of the first interval with |i| > threshold.

    start is the first sample above the threshold, end the next sample at or below it. The interval is
    [t[start], t[end]). problem is None, or a reason the run has no usable interval.
    """
    above = np.abs(i) > threshold
    if above[0]:
        return None, None, f"starts above the threshold ({i[0]:.3f} A at t={t[0]:.4f} s)"
    rises = np.flatnonzero(above)
    if rises.size == 0:
        return None, None, f"never goes above the threshold (max |I| {np.abs(i).max():.3f} A)"
    start = rises[0]
    falls = np.flatnonzero(~above[start:])
    if falls.size == 0:
        return None, None, f"goes above the threshold at t={t[start]:.4f} s and never falls back below it"
    return start, start + falls[0], None


def count_events(i, threshold):
    above = (np.abs(i) > threshold).astype(np.int8)
    return int(np.count_nonzero(np.diff(above) == 1) + above[0])


class NotARun(Exception):
    pass


def read_run(csv_path, channel):
    """Returns (header, rows, t, i, gap). rows are the CSV's own text fields, minus rows that don't parse."""
    with open(csv_path, newline="") as f:
        reader = csv.reader(f)
        header = next(reader, [])
        if "t_s" not in header or channel not in header:
            raise NotARun(f"not a fuse_logger run CSV (needs columns t_s and {channel})")
        cols = [header.index("t_s"), header.index(channel), header.index("gap") if "gap" in header else None]
        rows, values = [], []
        for row in reader:  # a run cut off mid-write can end in a partial row
            try:
                values.append([float(row[c]) if c is not None else 1.0 for c in cols])
            except (ValueError, IndexError):
                continue
            rows.append(row)
    values = np.array(values, dtype=float).reshape(-1, 3)
    keep = np.isfinite(values).all(axis=1)
    rows = [row for row, k in zip(rows, keep) if k]
    t, i, gap = values[keep].T
    return header, rows, t, i, gap


def analyze(csv_path, threshold, channel):
    """Returns (blow_time_s, rms_A, warnings, event); the first two are None when the run has no usable
    interval. event is (header, rows) with only the interval's samples, or None.
    """
    header, rows, t, i, gap = read_run(csv_path, channel)
    if t.size < 2:
        return None, None, ["fewer than 2 samples"], None

    start, end, problem = first_event(t, i, threshold)
    if problem:
        return None, None, [problem], None

    # Each sample holds until the next one, so the weights sum to exactly the blow time and a missed
    # conversion doesn't skew the RMS
    dt = np.diff(t[start : end + 1])
    blow_time = t[end] - t[start]
    rms = float(np.sqrt(np.sum(i[start:end] ** 2 * dt) / blow_time))

    warnings = []
    n_events = count_events(i, threshold)
    if n_events > 1:
        later_peak = np.max(np.abs(i[end:]))
        warnings.append(
            f"{n_events} intervals above the threshold; only the first ({t[start]:.4f}-{t[end]:.4f} s) is used, "
            f"later peak |I| {later_peak:.1f} A"
        )
    if np.any(gap[start : end + 1] > 1):
        warnings.append("ADC conversions were missed during the interval")
    return blow_time, rms, warnings, (header, rows[start:end])


def relative_folders(runs):
    """Maps each run to its folder relative to the deepest folder all runs share ('' when they share one)."""
    folders = [os.path.abspath(r.parent) for r in runs]
    try:
        common = os.path.commonpath(folders)
    except ValueError:  # runs on different drives: nothing in common
        return {r: Path(f).as_posix() for r, f in zip(runs, folders)}
    return {r: "" if f == common else Path(os.path.relpath(f, common)).as_posix() for r, f in zip(runs, folders)}


def runs_in(path):
    path = Path(path)
    if path.is_dir():
        # Skip detailed-mode output so it isn't re-analyzed if it was saved under the logs folder
        return sorted(p for p in path.rglob("*.csv") if p.is_file() and not p.stem.endswith(TRUNCATED_SUFFIX))
    return [path]


def pick_inputs_dialog(mode, max_blow_time):
    """Window for choosing the mode, the blow-time limit and the run files/folders. Returns
    (inputs, mode, max_blow_time); inputs is [] if the window is closed.
    """
    import tkinter as tk
    from tkinter import filedialog

    initial = DEFAULT_LOG_DIR if DEFAULT_LOG_DIR.is_dir() else Path.cwd()
    chosen = []
    result = []

    root = tk.Tk()
    root.title("Fuse analysis: choose runs")
    root.attributes("-topmost", True)
    root.after(200, lambda: root.attributes("-topmost", False))

    mode_var = tk.StringVar(value=mode)
    modes = tk.Frame(root)
    modes.pack(fill=tk.X, padx=8, pady=(8, 0))
    tk.Label(modes, text="Mode:").pack(anchor="w")
    for text, value in (
        ("Overview (one summary CSV)", "overview"),
        ("Detailed (each run's blow interval in its own CSV)", "detailed"),
        ("Overlay (plot all runs from t=0)", "overlay"),
        ("Fuse curve (blow time vs RMS current, log-log)", "curve"),
    ):
        tk.Radiobutton(modes, text=text, variable=mode_var, value=value).pack(anchor="w", padx=(12, 0))

    limit = tk.Frame(root)
    limit.pack(fill=tk.X, padx=8, pady=(6, 0))
    tk.Label(limit, text="Discard runs with a blow time over").pack(side=tk.LEFT)
    limit_var = tk.StringVar(value="" if max_blow_time is None else f"{max_blow_time:g}")
    tk.Entry(limit, textvariable=limit_var, width=8).pack(side=tk.LEFT, padx=4)
    tk.Label(limit, text="s  (blank keeps all runs)").pack(side=tk.LEFT)
    limit_error = tk.Label(limit, fg="red")
    limit_error.pack(side=tk.LEFT, padx=8)
    limit_value = [max_blow_time]

    tk.Label(root, text="Run CSVs and folders to analyze (folders are searched recursively):").pack(
        anchor="w", padx=8, pady=(8, 2)
    )
    listbox = tk.Listbox(root, width=90, height=14, selectmode=tk.EXTENDED)
    listbox.pack(fill=tk.BOTH, expand=True, padx=8)
    buttons = tk.Frame(root)
    buttons.pack(fill=tk.X, padx=8, pady=8)

    def add(paths):
        for path in paths:
            if path not in chosen:
                chosen.append(path)
                listbox.insert(tk.END, str(path) + (os.sep if path.is_dir() else ""))
        analyze_button.config(state=tk.NORMAL if chosen else tk.DISABLED)

    def add_files():
        files = filedialog.askopenfilenames(
            parent=root, title="Select run CSV files", initialdir=initial, filetypes=[("CSV", "*.csv")]
        )
        add([Path(f) for f in files])

    def add_folder():
        folder = filedialog.askdirectory(parent=root, title="Select a folder of runs", initialdir=initial)
        if folder:
            add([Path(folder)])

    def remove():
        for index in reversed(listbox.curselection()):
            listbox.delete(index)
            del chosen[index]
        analyze_button.config(state=tk.NORMAL if chosen else tk.DISABLED)

    def finish():
        text = limit_var.get().strip()
        try:
            limit_value[0] = float(text) if text else None
            if limit_value[0] is not None and not limit_value[0] > 0:
                raise ValueError
        except ValueError:
            limit_error.config(text="Enter a positive number of seconds, or leave it blank")
            return
        result.extend(chosen)
        root.destroy()

    tk.Button(buttons, text="Add files...", command=add_files).pack(side=tk.LEFT)
    tk.Button(buttons, text="Add folder...", command=add_folder).pack(side=tk.LEFT, padx=4)
    tk.Button(buttons, text="Remove selected", command=remove).pack(side=tk.LEFT)
    analyze_button = tk.Button(buttons, text="Analyze...", command=finish, state=tk.DISABLED)
    analyze_button.pack(side=tk.RIGHT)
    root.mainloop()
    return result, mode_var.get(), limit_value[0]


def pick_output_dialog(title="Save fuse analysis as", initialfile="fuse_analysis.csv", filetypes=(("CSV", "*.csv"),)):
    import tkinter as tk
    from tkinter import filedialog

    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)
    out = filedialog.asksaveasfilename(
        parent=root,
        title=title,
        initialdir=SCRIPT_DIR,
        initialfile=initialfile,
        defaultextension=Path(initialfile).suffix,
        filetypes=list(filetypes),
    )
    root.destroy()
    return Path(out) if out else None


def pick_output_folder_dialog():
    import tkinter as tk
    from tkinter import filedialog

    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)
    out = filedialog.askdirectory(parent=root, title="Folder to save the truncated runs in", initialdir=SCRIPT_DIR)
    root.destroy()
    return Path(out) if out else None


def write_truncated(output_dir, folder, run, event):
    header, rows = event
    path = output_dir.joinpath(folder, run.stem + TRUNCATED_SUFFIX + ".csv")
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)
    return path


def group_name(run):
    return run.stem.split("_", 1)[0]


def natural_key(text):
    """Sorts '3mm' before '10mm'."""
    return [int(part) if part.isdigit() else part.lower() for part in re.split(r"(\d+)", text)]


def plot_overlay(results, channel, output, show):
    import matplotlib

    if not show:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    groups = {}
    for run, _, _, _, event in results:
        if event is not None:
            groups.setdefault(group_name(run), []).append(event)
    if not groups:
        sys.exit("No run has a usable interval; nothing to plot.")

    names = sorted(groups, key=natural_key)
    cmap = plt.get_cmap("tab10" if len(names) <= 10 else "tab20")
    fig, ax = plt.subplots(figsize=(11, 6))
    for k, name in enumerate(names):
        color = cmap(k % cmap.N)
        for n, (header, rows) in enumerate(groups[name]):
            t = np.array([float(row[header.index("t_s")]) for row in rows])
            i = np.array([float(row[header.index(channel)]) for row in rows])
            label = f"{name} ({len(groups[name])})" if n == 0 else None
            ax.plot(t - t[0], i, lw=1, alpha=0.8, color=color, label=label)
    n_runs = sum(len(g) for g in groups.values())
    ax.set_title(f"Current vs Fuse Blow Time of {n_runs} run(s)")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel(f"Current {channel[:2].upper()} (A)")
    ax.grid(True, alpha=0.3)
    ax.legend(title="Group (runs)", loc="upper right")
    fig.tight_layout()
    if output is not None:
        output.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(output, dpi=150)
        print(f"Saved {output}")
    if show:
        plt.show()


def thermal_time(current, tau, i_min):
    return tau * np.log(current**2 / (current**2 - i_min**2))


def equivalent_current(t, i, blow_time, tau):
    """Constant current that leaves the fuse in the same thermal state at blow_time as the trace (t, i).

    Each sample holds until the next one (the last until blow_time), and du/dt = (I^2 - u) / tau is solved
    exactly over each step from u = 0.
    """
    t = t - t[0]
    ends = np.append(t[1:], blow_time)
    weights = -np.expm1(-np.diff(np.append(t, blow_time)) / tau) * np.exp(-(blow_time - ends) / tau)
    u = np.sum(i**2 * weights)
    return np.sqrt(u / -np.expm1(-blow_time / tau))


def fit_thermal(traces, blow_times):
    """Fits tau and I_min to a group of runs by a robust (Cauchy) fit on log(blow time).

    traces is a list of (t, i) arrays. For each tau on a grid, every run's equivalent current is computed and
    I_min is searched below the lowest one. Returns (tau, i_min, equivalent currents, inlier log_rms_error,
    pinned); pinned means I_min or tau sat at the edge of its search, i.e. the data don't locate it.
    """
    scale = np.log(OUTLIER_FACTOR)
    y = np.log(blow_times)
    taus = np.geomspace(0.05, 500, 121)
    best = None
    for tau in taus:
        current = np.array([equivalent_current(t, i, bt, tau) for (t, i), bt in zip(traces, blow_times)])
        i_mins = np.linspace(0.01, 0.999, 1000)[:, None] * current.min()
        predicted = np.log(tau * np.log(current**2 / (current**2 - i_mins**2)))
        costs = np.sum(np.log1p(((y - predicted) / scale) ** 2), axis=1)
        k = int(np.argmin(costs))
        if best is None or costs[k] < best[0]:
            best = (costs[k], tau, i_mins[k, 0], current, k == len(i_mins) - 1, y - predicted[k])
    _, tau, i_min, current, pinned, residual = best
    inliers = np.abs(residual) <= scale
    err = np.sqrt(np.mean(residual[inliers] ** 2)) if inliers.any() else float("nan")
    return tau, i_min, current, err, pinned or tau in (taus[0], taus[-1])


def plot_fuse_curves(results, channel, output, show):
    import matplotlib

    if not show:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.ticker import FuncFormatter

    groups = {}
    for run, blow_time, rms, _, event in results:
        if blow_time is not None:
            header, rows = event
            t = np.array([float(row[header.index("t_s")]) for row in rows])
            i = np.abs(np.array([float(row[header.index(channel)]) for row in rows]))
            groups.setdefault(group_name(run), []).append((rms, blow_time, (t, i), run.stem))
    if not groups:
        sys.exit("No run has a usable interval; nothing to plot.")

    names = sorted(groups, key=natural_key)
    cmap = plt.get_cmap("tab10" if len(names) <= 10 else "tab20")
    fig, ax = plt.subplots(figsize=(10, 7))
    print("Thermal model fits, t = tau * ln(I^2 / (I^2 - I_min^2)) at constant current:")
    for k_color, name in enumerate(names):
        color = cmap(k_color % cmap.N)
        rms = np.array([g[0] for g in groups[name]])
        time = np.array([g[1] for g in groups[name]])
        current = rms
        outlier = np.zeros(rms.size, dtype=bool)
        label = f"{name or '(no name)'}: {current.size} run(s)"
        if current.size >= MIN_FIT_POINTS and current.max() / current.min() >= MIN_FIT_SPAN:
            tau, i_min, current, err, pinned = fit_thermal([g[2] for g in groups[name]], time)
            measured = np.geomspace(current.min(), current.max(), 200)
            ax.plot(measured, thermal_time(measured, tau, i_min), color=color, lw=1.5)
            # Extrapolate towards the asymptote, stopping where it leaves the plotted time range
            below = np.geomspace(i_min * 1.0005, current.min(), 400)
            below_t = thermal_time(below, tau, i_min)
            keep = below_t <= 10 * time.max()
            ax.plot(below[keep], below_t[keep], color=color, lw=1, ls="--")
            label += f", I_min≈{i_min:.0f} A"
            note = "  (I_min or tau at the edge of its search: not located by the data)" if pinned else ""
            print(
                f"  {name:12s} n={current.size:2d}  I_min={i_min:6.1f} A  tau={tau:6.2f} s  "
                f"typical error of non-outliers x{np.exp(err):.2f}{note}"
            )
            predicted = thermal_time(current, tau, i_min)
            outlier = np.abs(np.log(predicted / time)) > np.log(OUTLIER_FACTOR)
            for (run_rms, bt, _, run_name), i_eq, pred, out in zip(groups[name], current, predicted, outlier):
                if out:
                    print(
                        f"      outlier {run_name}: {bt:.2f} s at I_eq {i_eq:.0f} A (RMS {run_rms:.0f} A), "
                        f"fit predicts {pred:.2f} s"
                    )
            shifted = np.abs(current / rms - 1) > 0.05
            ax.scatter(rms[shifted], time[shifted], s=22, facecolors="none", edgecolors=color, alpha=0.5, zorder=2)
            for x0, x1, y in zip(rms[shifted], current[shifted], time[shifted]):
                ax.plot([x0, x1], [y, y], color=color, lw=0.8, alpha=0.5)
        else:
            label += ", no fit (RMS shown)"
            print(
                f"  {name:12s} n={current.size:2d}  no fit: needs {MIN_FIT_POINTS}+ runs spanning x{MIN_FIT_SPAN:g} "
                f"in current (has {current.min():.0f}-{current.max():.0f} A)"
            )
        ax.scatter(current[~outlier], time[~outlier], color=color, s=28, zorder=3, label=label)
        ax.scatter(current[outlier], time[outlier], color=color, s=40, marker="x", zorder=3)

    ax.set_xscale("log")
    ax.set_yscale("log")
    plain = FuncFormatter(lambda value, _: f"{value:g}")
    for axis in (ax.xaxis, ax.yaxis):
        axis.set_major_formatter(plain)
        axis.set_minor_formatter(plain)
    ax.set_title("Estimated fuse curves (first blow interval of each run)")
    ax.set_xlabel("Equivalent constant current (A)")
    ax.set_ylabel("Blow time (s)")
    ax.grid(True, which="both", alpha=0.3)
    ax.scatter([], [], color="gray", marker="x", label=f"outlier: >x{OUTLIER_FACTOR:g} off its fit")
    ax.legend(title="Group", loc="upper right", fontsize=9)
    fig.tight_layout()
    if output is not None:
        output.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(output, dpi=150)
        print(f"Saved {output}")
    if show:
        plt.show()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("inputs", nargs="*", help="run CSVs and/or folders of runs (default: choose in dialogs)")
    parser.add_argument("--mode", choices=["overview", "detailed", "overlay", "curve"], default="overview", help="default: overview")
    parser.add_argument(
        "-o",
        "--output",
        help="overview: summary CSV; detailed: output folder; overlay/curve: image file (default: choose in a dialog)",
    )
    parser.add_argument("--no-show", action="store_true", help="overlay/curve: save the plot without opening it")
    parser.add_argument("--threshold", type=float, default=DEFAULT_THRESHOLD_A, help="current threshold in A (default 0.5)")
    parser.add_argument(
        "--max-blow-time", type=float, metavar="S", help="discard runs whose blow time is over S seconds (default: keep all)"
    )
    parser.add_argument("--channel", choices=["i1_A", "i2_A"], default="i1_A", help="current column to analyze (default i1_A)")
    args = parser.parse_args()

    if args.inputs:
        inputs, mode, max_blow_time = [Path(p) for p in args.inputs], args.mode, args.max_blow_time
    else:
        inputs, mode, max_blow_time = pick_inputs_dialog(args.mode, args.max_blow_time)
    if not inputs:
        sys.exit("No runs chosen; nothing written.")
    runs, seen = [], set()
    for path in inputs:
        if not path.exists():
            print(f"WARNING: {path} does not exist; skipped")
            continue
        for run in runs_in(path):
            key = run.resolve()
            if key not in seen:
                seen.add(key)
                runs.append(run)
    if not runs:
        sys.exit("No run CSVs found.")

    if args.output:
        output = Path(args.output)
    elif mode == "overview":
        output = pick_output_dialog()
    elif mode == "detailed":
        output = pick_output_folder_dialog()
    else:
        title, initialfile = ("overlay plot", "fuse_overlay.png") if mode == "overlay" else ("fuse curves", "fuse_curves.png")
        output = pick_output_dialog(
            f"Save {title} as (Cancel to only view it)", initialfile, (("PNG", "*.png"), ("PDF", "*.pdf"), ("SVG", "*.svg"))
        )
    if output is None and mode not in ("overlay", "curve"):
        sys.exit("No output chosen; nothing written.")
    if output is None and args.no_show:
        sys.exit("--no-show needs -o; nothing to do.")
    output_key = output.resolve() if output is not None else None

    limit_text = "" if max_blow_time is None else f", discarding blow times over {max_blow_time:g} s"
    print(f"{mode.capitalize()} mode, threshold {args.threshold} A on |{args.channel}|{limit_text}, {len(runs)} run(s)")
    results = []
    for run in runs:
        if run.resolve() == output_key:
            continue
        try:
            blow_time, rms, warnings, event = analyze(run, args.threshold, args.channel)
        except NotARun as exc:
            print(f"  skipped {run.name}: {exc}")
            continue
        except Exception as exc:  # one unreadable file shouldn't stop the batch
            blow_time, rms, warnings, event = None, None, [f"could not read: {exc}"], None
        if blow_time is not None and max_blow_time is not None and blow_time > max_blow_time:
            warnings = [f"discarded: blow time {blow_time:.3f} s is over the {max_blow_time:g} s limit"]
            blow_time, rms, event = None, None, None
        results.append((run, blow_time, rms, warnings, event))
        result = f"ERROR: {warnings[0]}" if blow_time is None else f"{blow_time * 1e3:9.1f} ms  {rms:8.2f} A rms"
        print(f"  {run.name:40s} {result}")
        for w in warnings if blow_time is not None else []:
            print(f"      WARNING: {w}")

    if mode in ("overlay", "curve"):
        for run, _, _, warnings, event in results:
            if event is None:
                print(f"  not plotted {run.name}: {warnings[0]}")
        if mode == "overlay":
            plot_overlay(results, args.channel, output, show=not args.no_show)
        else:
            plot_fuse_curves(results, args.channel, output, show=not args.no_show)
        return

    folders = relative_folders([r[0] for r in results])
    if mode == "detailed":
        written = 0
        for run, blow_time, _, warnings, event in results:
            if event is None:
                print(f"  no file for {run.name}: {warnings[0]}")
                continue
            write_truncated(output, folders[run], run, event)
            written += 1
        print(f"Saved {written} truncated run(s) under {output}")
        return

    rows = [
        [
            folders[run],
            run.name,
            "" if blow_time is None else f"{blow_time:.6f}",
            "" if rms is None else f"{rms:.3f}",
            warnings[0] if blow_time is None else "",
        ]
        for run, blow_time, rms, warnings, _ in results
    ]

    output.parent.mkdir(parents=True, exist_ok=True)
    with open(output, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(OUT_COLUMNS)
        writer.writerows(rows)
    print(f"Saved {len(rows)} row(s) to {output}")


if __name__ == "__main__":
    main()

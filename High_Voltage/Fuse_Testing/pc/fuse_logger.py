#!/usr/bin/env python3
"""Fuse test current logger (PC side).

Receives the NUCLEO-G474RE / ADBMS2950B sample stream, writes every run to its own CSV in
Fuse_Testing/logs/, shows a live plot while logging, and plots the finished run as
current (A) vs time (s).

    python fuse_logger.py                 # asks for a run name, then logs (auto-detects the COM port)
    python fuse_logger.py --port COM5     # pick the port explicitly
    python fuse_logger.py --plot          # plot the most recent run
    python fuse_logger.py --plot ..\\logs\\30A_slowblow_20261029_041500.csv

A run ends when you close the live plot window or press Ctrl+C.
"""

import argparse
import csv
import datetime as dt
import importlib.util
import re
import signal
import struct
import sys
import threading
import time
from collections import deque
from pathlib import Path

_missing = [pip for mod, pip in (("serial", "pyserial"), ("numpy", "numpy"), ("matplotlib", "matplotlib"))
            if importlib.util.find_spec(mod) is None]
if _missing:
    sys.exit(
        f"Missing package(s) {', '.join(_missing)} for this Python ({sys.executable}).\n"
        f'Install them with:  "{sys.executable}" -m pip install -r "{Path(__file__).with_name("requirements.txt")}"\n'
        "or run the script with a Python that has them."
    )

import serial  # noqa: E402
import serial.tools.list_ports  # noqa: E402

if not hasattr(serial, "serial_for_url"):
    # The unrelated PyPI package "serial" installs a module with the same name over pyserial's
    sys.exit(
        "The 'serial' module here is not pyserial (the PyPI package 'serial' is installed). Fix with:\n"
        f'  "{sys.executable}" -m pip uninstall -y serial pyserial\n'
        f'  "{sys.executable}" -m pip install pyserial'
    )

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_LOG_DIR = SCRIPT_DIR.parent / "logs"
DEFAULT_BAUD = 1_000_000
ST_VID = 0x0483  # STMicroelectronics (ST-LINK virtual COM port)

# Must match firmware/App/inc/stream.h
SYNC = b"\xA5\x5A"
TYPE_SAMPLE, TYPE_TEXT, TYPE_STATS, TYPE_START = 0x01, 0x02, 0x03, 0x04
SAMPLE_FMT = struct.Struct("<IIHiiBB")
STATS_FMT = struct.Struct("<8I")
START_FMT = struct.Struct("<IfffbBBB")
FLAG_GAP, FLAG_UNCAL, FLAG_RETRIED = 0x01, 0x02, 0x04

# Used until the firmware's START frame arrives; must match firmware/App/inc/app_config.h
DEFAULT_SHUNT_OHM = 50e-6
DEFAULT_I1_LSB_V = 1e-6
DEFAULT_I2_LSB_V = -1e-6

CSV_COLUMNS = ["t_s", "i1_A", "i2_A", "i1cnt", "seq", "flags", "gap", "i1_code", "i2_code"]


def crc16_ccitt(data, crc=0xFFFF):
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


class FrameParser:
    """Splits the byte stream into (type, payload) frames, resyncing on CRC errors."""

    def __init__(self):
        self.buf = bytearray()
        self.crc_errors = 0

    def feed(self, data):
        self.buf += data
        frames = []
        while True:
            start = self.buf.find(SYNC)
            if start < 0:
                del self.buf[:-1]
                return frames
            del self.buf[:start]
            if len(self.buf) < 4:
                return frames
            ftype, length = self.buf[2], self.buf[3]
            end = 4 + length + 2
            if len(self.buf) < end:
                return frames
            body = bytes(self.buf[2 : 4 + length])
            crc = self.buf[4 + length] | (self.buf[5 + length] << 8)
            if crc16_ccitt(body) == crc:
                frames.append((ftype, body[2:]))
                del self.buf[:end]
            else:
                self.crc_errors += 1
                del self.buf[:1]


def find_port():
    ports = list(serial.tools.list_ports.comports())
    st_ports = [p for p in ports if p.vid == ST_VID]
    if len(st_ports) == 1:
        return st_ports[0].device
    listing = "\n".join(f"  {p.device}: {p.description}" for p in ports) or "  (none)"
    reason = "No ST-LINK COM port found" if not st_ports else "More than one ST-LINK COM port"
    sys.exit(f"{reason}; pass --port. Available ports:\n{listing}")


class RunLogger:
    """Reads frames on a background thread and writes samples to the run's CSV."""

    def __init__(self, ser, csv_path, events_path, restart):
        self.ser = ser
        self.csv_path = csv_path
        self.events_path = events_path
        self.restart = restart
        self.parser = FrameParser()
        self.stop_event = threading.Event()
        self.lock = threading.Lock()
        self.live_t = deque(maxlen=20_000)
        self.live_i1 = deque(maxlen=20_000)
        self.live_i2 = deque(maxlen=20_000)
        self.scale = {"shunt": DEFAULT_SHUNT_OHM, "i1": DEFAULT_I1_LSB_V, "i2": DEFAULT_I2_LSB_V, "sign": 1}
        self.have_start = False
        self.t0_us = None
        self.last_raw_t = None
        self.t_wraps = 0
        self.last_seq = None
        self.n_samples = 0
        self.n_frames_lost = 0
        self.n_conv_missed = 0
        self.n_uncal = 0
        self.last_stats = None
        self.error = None

    def log_event(self, text):
        stamp = dt.datetime.now().strftime("%H:%M:%S.%f")[:-3]
        line = f"{stamp}  {text}"
        print(line)
        self.events.write(line + "\n")
        self.events.flush()

    def amps(self, i1_code, i2_code):
        s = self.scale
        return (s["sign"] * i1_code * s["i1"] / s["shunt"], i2_code * s["i2"] / s["shunt"])

    def handle_sample(self, payload, writer):
        seq, t_us, i1cnt, i1_code, i2_code, flags, gap = SAMPLE_FMT.unpack(payload)
        if self.restart and not self.have_start:
            return  # still flushing samples from before the restart

        if self.last_raw_t is not None and t_us < self.last_raw_t:
            self.t_wraps += 1
        self.last_raw_t = t_us
        t_abs = t_us + (self.t_wraps << 32)
        if self.t0_us is None:
            self.t0_us = t_abs
        t_s = (t_abs - self.t0_us) / 1e6

        if self.last_seq is not None and seq != (self.last_seq + 1) & 0xFFFFFFFF:
            lost = (seq - self.last_seq - 1) & 0xFFFFFFFF
            self.n_frames_lost += lost
            self.log_event(f"WARNING: {lost} sample frame(s) lost on the serial link before t={t_s:.4f} s")
        self.last_seq = seq
        if self.n_samples > 0 and flags & FLAG_GAP:
            self.n_conv_missed += gap - 1
            self.log_event(f"WARNING: {gap - 1} ADC conversion(s) missed before t={t_s:.4f} s")
        if flags & FLAG_UNCAL:
            self.n_uncal += 1

        i1_a, i2_a = self.amps(i1_code, i2_code)
        writer.writerow([f"{t_s:.6f}", f"{i1_a:.3f}", f"{i2_a:.3f}", i1cnt, seq, flags, gap, i1_code, i2_code])
        self.n_samples += 1
        with self.lock:
            self.live_t.append(t_s)
            self.live_i1.append(i1_a)
            self.live_i2.append(i2_a)

    def handle_start(self, payload):
        _, shunt, i1_lsb, i2_lsb, sign, rd, devid, i1cal = START_FMT.unpack(payload)
        self.scale = {"shunt": shunt, "i1": i1_lsb, "i2": i2_lsb, "sign": sign}
        if self.have_start:
            self.log_event("WARNING: firmware restarted the measurement mid-run")
        self.have_start = True
        self.log_event(
            f"Measurement started: shunt={shunt * 1e6:.3f} uOhm, I1 sign={sign:+d}, "
            f"I2 {'on' if rd else 'off'}, DEVID=0x{devid:02X}, I1CAL={i1cal}"
        )

    def run(self):
        try:
            with open(self.csv_path, "w", newline="") as f, open(self.events_path, "w") as self.events:
                writer = csv.writer(f)
                writer.writerow(CSV_COLUMNS)
                self.log_event(f"Logging to {self.csv_path}")
                self.ser.reset_input_buffer()
                if self.restart:
                    self.ser.write(b"R")
                last_flush = time.monotonic()
                while not self.stop_event.is_set():
                    data = self.ser.read(self.ser.in_waiting or 1)
                    for ftype, payload in self.parser.feed(data):
                        if ftype == TYPE_SAMPLE and len(payload) == SAMPLE_FMT.size:
                            self.handle_sample(payload, writer)
                        elif ftype == TYPE_START and len(payload) == START_FMT.size:
                            self.handle_start(payload)
                        elif ftype == TYPE_STATS and len(payload) == STATS_FMT.size:
                            self.last_stats = STATS_FMT.unpack(payload)
                        elif ftype == TYPE_TEXT:
                            self.log_event("MCU: " + payload.decode("ascii", "replace"))
                    if time.monotonic() - last_flush > 1.0:
                        f.flush()
                        last_flush = time.monotonic()
                self.log_event(self.summary())
        except Exception as exc:  # surface reader errors to the main thread
            self.error = exc
            self.stop_event.set()

    def summary(self):
        text = (
            f"Run finished: {self.n_samples} samples, {self.n_frames_lost} frames lost on the link, "
            f"{self.n_conv_missed} ADC conversions missed, {self.parser.crc_errors} CRC errors"
        )
        if self.n_uncal:
            text += f", {self.n_uncal} samples before I1ADC calibration finished"
        if self.last_stats:
            _, sent, missed, retries, pec, spi, dropped, reinit = self.last_stats
            text += (
                f"\nMCU totals since boot: sent={sent} missed={missed} retries={retries} "
                f"pec_errors={pec} spi_errors={spi} tx_dropped={dropped} reinit={reinit}"
            )
        return text


def live_plot(logger, window_s):
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation

    fig, ax = plt.subplots(figsize=(11, 5))
    (line1,) = ax.plot([], [], lw=1, label="I1")
    (line2,) = ax.plot([], [], lw=1, alpha=0.6, label="I2 (redundant)")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Current (A)")
    ax.set_title(f"Live: {logger.csv_path.name}  (close window to end run)")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper left")

    def update(_):
        if logger.stop_event.is_set():
            plt.close(fig)
            return line1, line2
        with logger.lock:
            t = list(logger.live_t)
            i1 = list(logger.live_i1)
            i2 = list(logger.live_i2)
        if t:
            line1.set_data(t, i1)
            line2.set_data(t, i2)
            ax.set_xlim(max(0.0, t[-1] - window_s), max(window_s, t[-1]))
            lo, hi = min(min(i1), min(i2)), max(max(i1), max(i2))
            pad = max(1.0, 0.05 * (hi - lo))
            ax.set_ylim(lo - pad, hi + pad)
        return line1, line2

    # Keep a reference so the animation is not garbage collected while the window is open
    fig._anim = FuncAnimation(fig, update, interval=100, cache_frame_data=False)
    plt.show()


def plot_run(csv_path, show=True):
    import matplotlib

    if not show:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np

    data = np.genfromtxt(csv_path, delimiter=",", names=True)
    if data.size == 0:
        print(f"{csv_path} has no samples")
        return
    data = np.atleast_1d(data)
    t, i1, i2 = data["t_s"], data["i1_A"], data["i2_A"]

    fig, ax = plt.subplots(figsize=(11, 5))
    ax.plot(t, i1, lw=1, label="I1")
    if np.any(data["i2_code"] != 0):
        ax.plot(t, i2, lw=1, alpha=0.6, label="I2 (redundant)")
    peak = i1[np.argmax(np.abs(i1))]
    rate = (len(t) - 1) / (t[-1] - t[0]) if len(t) > 1 and t[-1] > t[0] else float("nan")
    ax.set_title(f"{Path(csv_path).name}: {len(t)} samples, {rate:.0f} S/s, peak I1 {peak:.1f} A")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Current (A)")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper left")
    fig.tight_layout()
    png = Path(csv_path).with_suffix(".png")
    fig.savefig(png, dpi=150)
    print(f"Saved {png}")
    if show:
        plt.show()


def latest_run(log_dir):
    runs = sorted(Path(log_dir).rglob("*.csv"), key=lambda p: p.stat().st_mtime)
    if not runs:
        sys.exit(f"No runs in {log_dir}")
    return runs[-1]


def clean_run_name(name):
    """Splits 'calibration/test 1' into ['calibration', 'test_1'].

    '/' or '\\' separate subfolders under the log directory. In each part spaces become '_' and
    anything other than letters, digits, '-' and '_' is dropped; empty parts (and '..') vanish.
    """
    parts = []
    for part in re.split(r"[/\\]", name.strip()):
        part = re.sub(r"[^A-Za-z0-9_-]", "", part.strip().replace(" ", "_"))
        if part:
            parts.append(part)
    return parts


def ask_run_name(given):
    name = given
    while True:
        if name is None:
            name = input("Run name (use / for subfolders, e.g. calibration/test1): ")
        parts = clean_run_name(name)
        if parts:
            if "/".join(parts) != name.strip().replace("\\", "/"):
                print(f"Using run name '{'/'.join(parts)}'")
            return parts
        print("Enter a name using letters, digits, '-' or '_'.")
        name = None


def cmd_log(args):
    port = args.port or find_port()
    *subdirs, name = ask_run_name(args.name)
    log_dir = Path(args.log_dir).joinpath(*subdirs)
    log_dir.mkdir(parents=True, exist_ok=True)
    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_path = log_dir / f"{name}_{stamp}.csv"
    events_path = log_dir / f"{name}_{stamp}_events.txt"

    ser = serial.serial_for_url(port, baudrate=args.baud, timeout=0.05)
    logger = RunLogger(ser, csv_path, events_path, restart=not args.no_restart)
    reader = threading.Thread(target=logger.run, daemon=True)
    reader.start()

    # Ctrl+Break (Windows) and SIGTERM end the run like Ctrl+C, so the CSV and summary are finished
    def end_run(signum, frame):
        raise KeyboardInterrupt

    for name in ("SIGBREAK", "SIGTERM"):
        if hasattr(signal, name):
            signal.signal(getattr(signal, name), end_run)

    try:
        if args.no_live:
            print("Logging; press Ctrl+C to end the run.")
            while reader.is_alive():
                time.sleep(0.2)
        else:
            live_plot(logger, args.window)
    except KeyboardInterrupt:
        pass
    finally:
        logger.stop_event.set()
        reader.join(timeout=5)
        ser.close()

    if logger.error:
        sys.exit(f"Logging stopped with an error: {logger.error!r}")
    plot_run(csv_path, show=not args.no_plot)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", help="serial port, e.g. COM5 (default: auto-detect ST-LINK)")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--name", help="run name (skips the prompt), e.g. calibration/test1 -> logs/calibration/test1_<YYYYMMDD_HHMMSS>.csv")
    parser.add_argument("--log-dir", default=str(DEFAULT_LOG_DIR), help="where runs are saved")
    parser.add_argument("--no-live", action="store_true", help="no live plot; end the run with Ctrl+C")
    parser.add_argument("--window", type=float, default=10.0, help="live plot window (s)")
    parser.add_argument("--no-plot", action="store_true", help="save the run plot as PNG without opening it")
    parser.add_argument(
        "--no-restart", action="store_true", help="don't restart the ADBMS2950B measurement at run start"
    )
    parser.add_argument(
        "--plot", nargs="?", const="latest", metavar="CSV", help="plot a saved run instead of logging (default: latest)"
    )
    args = parser.parse_args()

    if args.plot:
        csv_path = latest_run(args.log_dir) if args.plot == "latest" else args.plot
        plot_run(csv_path, show=not args.no_plot)
    else:
        cmd_log(args)


if __name__ == "__main__":
    main()

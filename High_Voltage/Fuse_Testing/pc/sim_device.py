#!/usr/bin/env python3
"""Simulated Nucleo for testing fuse_logger.py without hardware.

Serves the firmware's frame stream on a TCP port at ~1 kS/s with a synthetic fuse event
(current steps to 400 A at t=1 s and the fuse opens at t=2 s).

    python sim_device.py --tcp 7777
    python fuse_logger.py --port socket://localhost:7777
"""

import argparse
import math
import random
import socket
import struct
import time

from fuse_logger import SAMPLE_FMT, START_FMT, STATS_FMT, SYNC, TYPE_SAMPLE, TYPE_START, TYPE_STATS, TYPE_TEXT
from fuse_logger import crc16_ccitt

SHUNT = 50e-6


def frame(ftype, payload):
    body = bytes([ftype, len(payload)]) + payload
    crc = crc16_ccitt(body)
    return SYNC + body + bytes([crc & 0xFF, crc >> 8])


def current_at(t):
    if t < 1.0:
        return 2.0
    if t < 2.0:
        return 400.0 * (1 + 0.05 * math.sin(2 * math.pi * 50 * t))
    return 0.0


def serve(conn):
    conn.setblocking(False)
    t_start = time.monotonic()
    seq = 0
    cnt = 0
    run_t0 = time.monotonic()
    next_stats = 1.0
    conn.sendall(frame(TYPE_TEXT, b"Simulated ADBMS2950B"))
    while True:
        try:
            if b"R" in conn.recv(64):
                run_t0 = time.monotonic()
                cnt = 0
                conn.sendall(frame(TYPE_TEXT, b"Restart requested by PC"))
                conn.sendall(frame(TYPE_START, START_FMT.pack(0, SHUNT, 1e-6, -1e-6, 1, 1, 6, 1)))
        except BlockingIOError:
            pass
        now = time.monotonic()
        t_us = int((now - t_start) * 1e6) & 0xFFFFFFFF
        amps = current_at(now - run_t0) + random.gauss(0, 0.05)
        i1 = round(amps * SHUNT / 1e-6)
        i2 = -round(amps * SHUNT / 1e-6)
        gap = 2 if random.random() < 0.001 else 1  # occasional missed conversion
        cnt = (cnt + gap) & 0x7FF
        conn.sendall(frame(TYPE_SAMPLE, SAMPLE_FMT.pack(seq, t_us, cnt, i1, i2, 1 if gap > 1 else 0, gap)))
        seq += 1
        if now - t_start > next_stats:
            conn.sendall(frame(TYPE_STATS, STATS_FMT.pack(t_us, seq, 0, 0, 0, 0, 0, 0)))
            next_stats += 1.0
        time.sleep(0.001)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tcp", type=int, default=7777)
    args = parser.parse_args()
    with socket.create_server(("localhost", args.tcp)) as srv:
        print(f"Simulated device on socket://localhost:{args.tcp}")
        while True:
            conn, _ = srv.accept()
            try:
                serve(conn)
            except (BrokenPipeError, ConnectionResetError):
                print("Client disconnected")


if __name__ == "__main__":
    main()

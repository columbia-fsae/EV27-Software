"""Reproduces track_layout.m: reconstruct and plot a track's (x, y) centerline directly
from its segment lengths/curvatures, without running any lap simulation.
"""
import sys
from pathlib import Path

import matplotlib.pyplot as plt

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lap_sim import ENDURANCE_EVENT, ICAHN_LOOP
from lap_sim import plotting


def main():
    fig, (ax_icahn, ax_endur) = plt.subplots(1, 2, figsize=(14, 7))
    plotting.plot_track_geometry(ICAHN_LOOP, ax=ax_icahn)
    plotting.plot_track_geometry(ENDURANCE_EVENT, ax=ax_endur)
    fig.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()

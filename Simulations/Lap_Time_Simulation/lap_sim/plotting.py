"""Matplotlib visualizations for lap results and design-study sweeps.

Reproduces the figures from `plot_lap`/`track_map` (in the `point_mass_sim_*.m` scripts)
and from `test_graph.m`/`test_graph_energy.m`/`track_layout.m`. Legend/styling choices
(e.g. `track_layout.m`'s random per-bucket coloring) are cleaned up rather than
pixel-matched; the underlying data and geometry are unchanged.
"""
from __future__ import annotations

from typing import Optional, Sequence

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.collections import LineCollection
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401 (registers the 3d projection)

from .geometry import integrate_path
from .results import LapResult
from .track import Track


def plot_lap_overview(results: Sequence[LapResult], labels: Optional[Sequence[str]] = None):
    """The velocity/brakes/tractive-power/electric-power/ax/ay grid, one or more cars
    overlaid on the same axes (the original scripts always plotted a single car here,
    but structured the code as if comparing two — this generalizes that intent).
    """
    if labels is None:
        labels = [r.car.name for r in results]

    fig, axes = plt.subplots(3, 2, figsize=(11, 12))
    panels = [
        ("Velocity (km/h)", lambda r: r.vv * 3.6),
        ("Brakes (kW)", lambda r: r.stats.pbrakes * 1e-3),
        ("Tractive Power (kW)", lambda r: r.stats.ptraction * 1e-3),
        ("Electric Power (kW)", lambda r: r.stats.pelectric * 1e-3),
        ("Longitudinal G Force (G)", lambda r: r.stats.ax / 9.806),
        ("Lateral G Force (G)", lambda r: r.stats.ay / 9.806),
    ]
    for ax, (ylabel, extract) in zip(axes.flat, panels):
        for result, label in zip(results, labels):
            ax.plot(result.x, extract(result), label=label)
        ax.set_xlabel("Distance (m)")
        ax.set_ylabel(ylabel)
        if "G Force" in ylabel:
            ax.set_ylim(-2, 2)
    axes.flat[1].legend()
    fig.tight_layout()

    track_fig, track_axes = plt.subplots(1, len(results), figsize=(6 * len(results), 5), squeeze=False)
    for ax, result, label in zip(track_axes[0], results, labels):
        plot_track_map(result, ax=ax)
        ax.set_title(label)
    track_fig.tight_layout()

    return fig, track_fig


def _points_per_lap(result: LapResult) -> int:
    """Number of simulation points in one lap.

    `LapSimulator.run` picks `dx` so `total_length / dx` is an integer, so every lap of
    a multi-lap (`run_multi_lap`) result has exactly this many points, at exactly the
    same physical locations, in the same order.
    """
    return int(round(result.track.total_length / result.dx))


def _plot_colored_line(ax, x, y, values, cmap, linewidth=2.0):
    """Draw (x, y) as a single connected line, colored per-segment by `values`.

    A plain `scatter` renders each point as an independent dot, which -- especially on
    a track with tight corners where points bunch up -- can look like disconnected
    clusters rather than the one continuous closed path it actually is. Connecting the
    points with a line removes that ambiguity.
    """
    points = np.column_stack([x, y]).reshape(-1, 1, 2)
    segments = np.concatenate([points[:-1], points[1:]], axis=1)
    lc = LineCollection(segments, cmap=cmap)
    lc.set_array(values[:-1])
    lc.set_linewidth(linewidth)
    ax.add_collection(lc)
    ax.margins(0.05)
    ax.autoscale_view()
    return lc


def plot_track_map(result: LapResult, ax=None, cmap: str = "viridis", points_per_segment: int = 200):
    """Track centerline colored by the simulated speed at each point.

    The shape is built from the track's own segment lengths/curvatures (as
    `plot_track_geometry` does), not by integrating the simulation's per-point (dx,
    curvature) grid: at a coarse simulation step (e.g. the endurance run's dx=0.5 m),
    snapping each segment's boundary to the nearest grid point measurably misallocates
    arc length between segments, and integrating that discretized curvature over a full
    lap can land tens of meters away from the true closed loop instead of retracing it.
    The track's segment table has no such grid to snap to, so integrating it directly
    (finely resampled) closes to within millimeters. Simulated speed, which *is* only
    known on the coarse dx grid, is then interpolated onto this accurate geometry by
    distance around the lap.

    For a multi-lap result, every lap lands on the same n-points-per-lap grid (see
    `_points_per_lap`), so laps are overlaid by averaging speed per point first.
    """
    ax = ax or plt.gca()
    track = result.track
    n = _points_per_lap(result)
    speed = result.vv.reshape(-1, n).mean(axis=0)
    x_speed = np.arange(n) * result.dx

    step = np.repeat(track.lengths / points_per_segment, points_per_segment)
    curvature = np.repeat(track.curvature, points_per_segment)
    x, y = integrate_path(step, curvature)
    s_geo = np.cumsum(step)
    speed_geo = np.interp(s_geo, x_speed, speed, period=track.total_length)

    lc = _plot_colored_line(ax, x, y, speed_geo, cmap)
    ax.set_aspect("equal")
    ax.grid(True)
    cbar = plt.colorbar(lc, ax=ax, orientation="horizontal")
    cbar.set_label("Speed (m/s)" if len(result.vv) == n else "Speed (m/s), averaged across laps")
    return ax


def plot_lap_speed_traces(result: LapResult, ax=None, cmap: str = "viridis"):
    """Speed vs. distance around the lap, one line per lap, tinted by lap number.

    For a multi-lap (`run_multi_lap`) result, this is the direct way to see per-lap
    trends (e.g. a degrading battery slowing later laps) that a single overlaid track
    map averages away.
    """
    ax = ax or plt.gca()
    n = _points_per_lap(result)
    n_laps = len(result.vv) // n
    x = result.x[:n]
    speed = result.vv.reshape(n_laps, n)
    colors = plt.get_cmap(cmap)(np.linspace(0, 1, n_laps))
    for row, color in zip(speed, colors):
        ax.plot(x, row, color=color, linewidth=0.8)
    ax.set_xlabel("Distance around lap (m)")
    ax.set_ylabel("Speed (m/s)")
    if n_laps > 1:
        sm = plt.cm.ScalarMappable(cmap=cmap, norm=plt.Normalize(1, n_laps))
        cbar = plt.colorbar(sm, ax=ax)
        cbar.set_label("Lap")
    return ax


def plot_track_geometry(track: Track, points_per_segment: int = 200, ax=None, cmap: str = "turbo"):
    """Track centerline on its own, colored by progress around the lap."""
    ax = ax or plt.gca()
    step = np.repeat(track.lengths / points_per_segment, points_per_segment)
    curvature = np.repeat(track.curvature, points_per_segment)
    x, y = integrate_path(step, curvature)
    progress = np.linspace(0, 1, len(x))
    lc = _plot_colored_line(ax, x, y, progress, cmap)
    ax.set_aspect("equal")
    ax.grid(True)
    ax.set_title(track.description)
    cbar = plt.colorbar(lc, ax=ax)
    cbar.set_label("Fraction of lap completed")
    return ax

def plot_battery_stats(results: Sequence[LapResult], labels: Optional[Sequence[str]] = None):
    if labels is None:
        labels = [r.car.name for r in results]

    fig, axes = plt.subplots(3, 2, figsize=(11, 16))
    panels = [
        ("Current (A)", lambda r: r.stats.batt_i),
        ("Battery Terminal V (V)", lambda r: r.stats.batt_v),
        ("Battery SOC (%)", lambda r: r.stats.batt_soc),
        ("Cell Temp (C)", lambda r: r.stats.cell_T),
        ("Battery Power Limit (kW)", lambda r: r.stats.batt_p_limit * 1e-3),
    ]
    for ax, (ylabel, extract) in zip(axes.flat, panels):
        for result, label in zip(results, labels):
            ax.plot(result.stats.tt, extract(result), label=label)
        ax.set_xlabel("Time (s)")
        ax.set_ylabel(ylabel)
    axes.flat[1].legend()
    axes.flat[5].axis("off")
    fig.tight_layout()
    
    return fig


def plot_heatmap(x_vals, y_vals, grid, xlabel, ylabel, cbar_label, title, ax=None):
    """Reproduces the `imagesc` figures in test_graph.m/test_graph_energy.m."""
    ax = ax or plt.gca()
    mesh = ax.pcolormesh(x_vals, y_vals, grid, shading="nearest")
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    cbar = plt.colorbar(mesh, ax=ax)
    cbar.set_label(cbar_label)
    return ax


def plot_surface(x_vals, y_vals, grid, xlabel, ylabel, zlabel, title):
    """Reproduces the `surf` figures in test_graph.m/test_graph_energy.m."""
    fig = plt.figure(figsize=(9, 7))
    ax = fig.add_subplot(projection="3d")
    xx, yy = np.meshgrid(x_vals, y_vals)
    surf = ax.plot_surface(xx, yy, grid, edgecolor="k", linewidth=0.3)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_zlabel(zlabel)
    ax.set_title(title)
    fig.colorbar(surf, ax=ax, shrink=0.6)
    return fig, ax


def plot_energy_density_lines(ax, energy_range, mass_intercepts, densities_wh_per_kg, density_labels=None):
    """Overlay `energy = density * (mass - intercept)` lines, as in test_graph_energy.m:
    for each candidate battery energy density and each mass offset, the line of
    (energy, mass) pairs achievable at that density.
    """
    density_labels = density_labels or [str(d) for d in densities_wh_per_kg]
    x_vals = np.linspace(min(energy_range), max(energy_range), 200)
    colors = plt.cm.tab10(np.linspace(0, 1, len(densities_wh_per_kg)))
    for density, color, label in zip(densities_wh_per_kg, colors, density_labels):
        for j, intercept in enumerate(mass_intercepts):
            y_vals = x_vals / density + intercept
            ax.plot(
                x_vals, y_vals, linestyle="-" if j == 0 else "--", linewidth=0.8,
                color=color, label=label if j == 0 else None,
            )
    ax.legend(title="Energy density (Wh/kg)", fontsize=8)
    return ax

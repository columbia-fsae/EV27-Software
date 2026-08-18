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


def plot_track_map(result: LapResult, ax=None, cmap: str = "viridis"):
    """Track centerline colored by the simulated speed at each point."""
    ax = ax or plt.gca()
    x, y = integrate_path(result.dx, result.stats.k)
    scatter = ax.scatter(x, y, c=result.vv, cmap=cmap, s=4)
    ax.set_aspect("equal")
    ax.grid(True)
    cbar = plt.colorbar(scatter, ax=ax, orientation="horizontal")
    cbar.set_label("Speed (m/s)")
    return ax


def plot_track_geometry(track: Track, points_per_segment: int = 200, ax=None, cmap: str = "turbo"):
    """Track centerline on its own, colored by progress around the lap."""
    ax = ax or plt.gca()
    step = np.repeat(track.lengths / points_per_segment, points_per_segment)
    curvature = np.repeat(track.curvature, points_per_segment)
    x, y = integrate_path(step, curvature)
    progress = np.linspace(0, 1, len(x))
    scatter = ax.scatter(x, y, c=progress, cmap=cmap, s=3)
    ax.set_aspect("equal")
    ax.grid(True)
    ax.set_title(track.description)
    cbar = plt.colorbar(scatter, ax=ax)
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

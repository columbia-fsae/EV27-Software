"""Reconstructing a track's (x, y) centerline from step size + curvature.

Shared by `plotting.plot_track_map` (colored by simulated speed) and
`plotting.plot_track_geometry` (the track layout on its own), since both are the same
underlying arc-integration: walk forward in constant-length steps, following a circular
arc of the given (signed) curvature at each step, or a straight line where curvature is 0.
"""
from __future__ import annotations

import numpy as np


def integrate_path(step, curvature: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Reconstruct (x, y) positions given a per-point step size and curvature.

    `step` may be a scalar (constant step, e.g. a lap simulation's fixed `dx`) or an
    array the same length as `curvature` (variable step, e.g. resampling each track
    segment into a fixed number of points regardless of its length).
    """
    curvature = np.asarray(curvature, dtype=float)
    steps = np.broadcast_to(np.asarray(step, dtype=float), curvature.shape)

    x_out = np.empty_like(curvature)
    y_out = np.empty_like(curvature)

    x = y = theta = 0.0
    for i, (ds, k) in enumerate(zip(steps, curvature)):
        if k == 0.0:
            x += ds * np.cos(theta)
            y += ds * np.sin(theta)
        else:
            radius = -1.0 / k
            dtheta = -ds * k
            cx = x - radius * np.sin(theta)
            cy = y + radius * np.cos(theta)
            x = cx + radius * np.sin(theta + dtheta)
            y = cy - radius * np.cos(theta + dtheta)
            theta += dtheta
        x_out[i] = x
        y_out[i] = y

    return x_out, y_out

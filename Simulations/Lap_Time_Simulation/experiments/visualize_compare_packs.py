from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

df = pd.read_csv(Path(__file__).resolve().parent / "output_data" / "compare_packs_results.csv")

CELL_MASS = 0.07
CAR_MASS = 175 + 60
ENERGY_METRIC = "final"  # "final" = actual energy consumed over the 22 laps (recommended);
                          # "nominal" = the pack's rated capacity from battery.py (even across
                          # mass, since it doesn't depend on mass, but ignores how efficiently
                          # that capacity actually got used)
# pack_combo is stored as a numpy-array repr string, e.g. "[100   2]" -- split on
# whitespace after stripping the brackets to recover the (series, parallel) ints.
pack_combos = np.array([
    [int(x) for x in combo.strip("[]").split()] for combo in df['pack_combo']
])
series = np.unique(pack_combos[:, 0])
parallel = np.unique(pack_combos[:, 1])

mesh = np.zeros((len(series), len(parallel)))
mesh_accel = np.zeros((len(series), len(parallel)))
mesh_skidpad = np.zeros((len(series), len(parallel)))

for i, (series_count, parallel_count) in enumerate(pack_combos):
    desired_mass = np.round((CAR_MASS + 1.2*CELL_MASS * series_count * parallel_count) / 5) * 5
    if df['mass'][i] == desired_mass:
        mesh[np.where(series_count == series)[0], np.where(parallel_count == parallel)[0]] = df['total_points'][i]
        mesh_accel[np.where(series_count == series)[0], np.where(parallel_count == parallel)[0]] = df['accel_time'][i]
        mesh_skidpad[np.where(series_count == series)[0], np.where(parallel_count == parallel)[0]] = df['skidpad_time'][i]
fig1, ax1 = plt.subplots(figsize=(8,6))
mesh = ax1.pcolormesh(series, parallel, mesh.T, shading="nearest")
ax1.set_xlabel("Series Cells")
ax1.set_ylabel("Parallel Cells")
ax1.set_title("Competition Points at Realistic Mass for Pack Configs")
cbar = fig1.colorbar(mesh, ax=ax1)
cbar.set_label("Total Points with realistic mass")

fig2, ax2 = plt.subplots(figsize=(8,6))
mesh = ax2.pcolormesh(series, parallel, mesh_accel.T, shading="nearest")
ax2.set_xlabel("Series Cells")
ax2.set_ylabel("Parallel Cells")
ax1.set_title("Accel Times at Realistic Mass for Pack Configs")
cbar = fig2.colorbar(mesh, ax=ax2)
cbar.set_label("Accel Times with realistic mass")

fig3, ax3 = plt.subplots(figsize=(8,6))
mesh = ax3.pcolormesh(series, parallel, mesh_skidpad.T, shading="nearest")
ax3.set_xlabel("Series Cells")
ax3.set_ylabel("Parallel Cells")
ax3.set_title("Skidpad Times at Realistic Mass for Pack Configs")
cbar = fig3.colorbar(mesh, ax=ax3)
cbar.set_label("Skidpad Times with realistic mass")

energy_col = "pack_energy_kwh"
energy_wh = df[energy_col] * 1000.0  # kWh -> Wh, more readable scale
# --- grid 2: mass x energy -> points --------------------------------------------
# Energy is a simulation *output*, not a swept variable, so (mass, energy) pairs
# don't land on a regular grid -- tricontourf interpolates a filled contour over the
# actual (irregular) sample cloud instead of assuming even spacing.
fig4, ax4 = plt.subplots(figsize=(8,6))
ax4.scatter(df['mass_kg'],df['skidpad_time'])

fig4, ax4 = plt.subplots(figsize=(8, 6))
contour = ax4.tricontourf(energy_wh, df["mass_kg"], df["accel_time"], levels=20, cmap="viridis")
ax4.scatter(energy_wh, df["mass_kg"], c="k", s=6, alpha=0.3)
ax4.set_xlabel("Nominal pack energy (Wh)")
ax4.set_ylabel("Mass (kg)")
ax4.set_title("Accel Time vs. Mass and Energy")
fig4.colorbar(contour, ax=ax4, label="Accel time (s)")

energy_col = "pack_energy_kwh"
energy_wh = df[energy_col] * 1000.0  # kWh -> Wh, more readable scale
# --- grid 2: mass x energy -> points --------------------------------------------
# Energy is a simulation *output*, not a swept variable, so (mass, energy) pairs
# don't land on a regular grid -- tricontourf interpolates a filled contour over the
# actual (irregular) sample cloud instead of assuming even spacing.
fig5, ax5 = plt.subplots(figsize=(8, 6))
contour = ax5.tricontourf(energy_wh, df["mass_kg"], df["skidpad_time"], levels=20, cmap="viridis")
ax5.scatter(energy_wh, df["mass_kg"], c="k", s=6, alpha=0.3)
ax5.set_xlabel("Nominal pack energy (Wh)")
ax5.set_ylabel("Mass (kg)")
ax5.set_title("Skidpad Time vs. Mass and Energy")
fig5.colorbar(contour, ax=ax5, label="Skidpad time (s)")

plt.show()

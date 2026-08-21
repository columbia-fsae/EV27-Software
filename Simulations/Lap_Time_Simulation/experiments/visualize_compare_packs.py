import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

df = pd.read_csv("/mnt/c/Users/levin/OneDrive/Documents/GitHub/EV27-Software/Simulations/Lap_Time_Simulation/experiments/compare_packs_results.csv")

CELL_MASS = 0.07
CAR_MASS = 175 + 60

# pack_combo is stored as a numpy-array repr string, e.g. "[100   2]" -- split on
# whitespace after stripping the brackets to recover the (series, parallel) ints.
pack_combos = np.array([
    [int(x) for x in combo.strip("[]").split()] for combo in df['pack_combo']
])
series = np.unique(pack_combos[:, 0])
parallel = np.unique(pack_combos[:, 1])

mesh = np.zeros((len(series), len(parallel)))
for i, (series_count, parallel_count) in enumerate(pack_combos):
    desired_mass = np.round((CAR_MASS + 1.1 * CELL_MASS * series_count * parallel_count) / 5) * 5
    if df['mass'][i] == desired_mass:
        mesh[np.where(series_count == series)[0], np.where(parallel_count == parallel)[0]] = df['total_points'][i]
ax = plt.gca()
mesh = ax.pcolormesh(series, parallel, mesh.T, shading="nearest")
ax.set_xlabel("Series Cells")
ax.set_ylabel("Parallel Cells")
ax.set_title("Competition Points at Realistic Mass for Pack Configs")
cbar = plt.colorbar(mesh, ax=ax)
cbar.set_label("Total Points with realistic mass")
plt.show()

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# -----------------------------------------------------
# 1) Read and prepare data
# -----------------------------------------------------
file_path = "./uv_responsivity_and_spectral_effectiveness_data.csv"
df = pd.read_csv(file_path)

w = df["Wavelength (nm)"].values
uvc = df["UVC"].values
uvb = df["UVB"].values
uva = df["UVA"].values
eff_std = df["Relative Spectral Effectiveness"].values
eff_prot = df["Relative Spectral Effectiveness (eyes protected)"].values

# -----------------------------------------------------
# 2) Peak finding (with UVA “double-peak” averaging)
# -----------------------------------------------------
peak_uvc_idx = np.nanargmax(uvc)
peak_uvb_idx = np.nanargmax(uvb)

uva_ones = w[uva == 1.0]
if len(uva_ones) >= 2:
    avg_uva_wl = (uva_ones[0] + uva_ones[1]) / 2
    peak_uva_idx = np.abs(w - avg_uva_wl).argmin()
else:
    peak_uva_idx = np.nanargmax(uva)

peak_uvc_wl = w[peak_uvc_idx]
peak_uvb_wl = w[peak_uvb_idx]
peak_uva_wl = w[peak_uva_idx]

peak_uvc_eff_std = eff_std[peak_uvc_idx]
peak_uvc_eff_prot = eff_prot[peak_uvc_idx]
peak_uvb_eff_std = eff_std[peak_uvb_idx]
peak_uvb_eff_prot = eff_prot[peak_uvb_idx]
peak_uva_eff_std = eff_std[peak_uva_idx]
peak_uva_eff_prot = eff_prot[peak_uva_idx]

# -----------------------------------------------------
# 3) Plotting
# -----------------------------------------------------
colors = {
    "UVC": "#4d4d4d",  # darker grey
    "UVB": "#8a2be2",  # violet/purple
    "UVA": "#0b66f2",  # pleasing blue
}

fig, ax1 = plt.subplots(figsize=(10, 6))

# 3.1) Sensor responsivity curves
ax1.plot(w, uvc, color=colors["UVC"], linewidth=2, label="UVC Responsivity")
ax1.plot(w, uvb, color=colors["UVB"], linewidth=2, label="UVB Responsivity")
ax1.plot(w, uva, color=colors["UVA"], linewidth=2, label="UVA Responsivity")

# 3.2) Peaks: vertical lines + marker + annotate SpecEff std/prot
y_annot = 1.05
for wl, color, eff_s, eff_p, lbl in [
    (peak_uvc_wl, colors["UVC"], peak_uvc_eff_std, peak_uvc_eff_prot, "UVC Peak"),
    (peak_uvb_wl, colors["UVB"], peak_uvb_eff_std, peak_uvb_eff_prot, "UVB Peak"),
    (peak_uva_wl, colors["UVA"], peak_uva_eff_std, peak_uva_eff_prot, "UVA Peak"),
]:
    ax1.axvline(x=wl, color=color, linestyle="dotted", linewidth=2, label=lbl)
    ax1.scatter(wl, 1.0, color=color, zorder=5)
    ax1.text(
        wl + 2,
        y_annot,
        f"{eff_s:.4g} / {eff_p:.4g}",
        color=color,
        fontsize=10,
        ha="left",
        va="bottom",
    )

# 3.3) Primary axis styling & legend (resp + peaks) top-right
ax1.set_xlabel("Wavelength (nm)")
ax1.set_ylabel("Normalized Sensor Responsivity")
ax1.set_ylim(0, 1.1)

leg1 = ax1.legend(loc="upper right", bbox_to_anchor=(1, 0.8))
ax1.add_artist(leg1)

# 3.4) Spectral-effectiveness curves on log scale & second legend
ax2 = ax1.twinx()
ax2.plot(
    w,
    eff_std,
    color="grey",
    linestyle="--",
    linewidth=2,
    alpha=0.6,
    label="SpecEff (standard)",
)
ax2.plot(
    w,
    eff_prot,
    color="grey",
    linestyle="-.",
    linewidth=2,
    alpha=0.6,
    label="SpecEff (eyes protected)",
)
ax2.set_ylabel("Relative Spectral Effectiveness (Log)")
ax2.set_yscale("log")

leg2 = ax2.legend(loc="upper right")
ax2.add_artist(leg2)

# 3.5) Final touches
ax1.set_title("UV Responsivity and Spectral Effectiveness")
plt.tight_layout()
plt.show()

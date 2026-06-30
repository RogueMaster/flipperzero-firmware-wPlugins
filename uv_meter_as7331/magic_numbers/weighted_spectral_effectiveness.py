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
# eff = df["Relative Spectral Effectiveness"].values
eff = df["Relative Spectral Effectiveness (eyes protected)"].values

# We have two cutoffs (rounded to data points):
cutoff_uvc_uvb = 280  # exact: 280.7
cutoff_uvb_uva = 315  # exact: 315.5

# Calculate "effective" curves = Responsivity * Spectral Effectiveness
uvc_eff = uvc * eff
uvb_eff = uvb * eff
uva_eff = uva * eff


# -----------------------------------------------------
# 2) Weighted spectral effectiveness (in-band only)
# -----------------------------------------------------
def compute_weighted_eff_in_band(wl, resp, sp_eff, band_min, band_max):
    """
    Weighted spectral effectiveness = sum(resp * sp_eff) / sum(resp),
    but only for band_min <= wl < band_max (and resp>0).
    (using Responsivity as the weight)
    """
    mask = (wl >= band_min) & (wl < band_max) & (resp > 0)
    if mask.sum() == 0:
        return 0.0
    return (resp[mask] * sp_eff[mask]).sum() / resp[mask].sum()


wl_min = w[0]
wl_max = w[-1]

uvc_w_eff = compute_weighted_eff_in_band(w, uvc, eff, wl_min, cutoff_uvc_uvb)
uvb_w_eff = compute_weighted_eff_in_band(w, uvb, eff, cutoff_uvc_uvb, cutoff_uvb_uva)
uva_w_eff = compute_weighted_eff_in_band(w, uva, eff, cutoff_uvb_uva, wl_max)


# -----------------------------------------------------
# 2.5) Compute Area Ratios and Correct Weighted Spectral Effectiveness
# -----------------------------------------------------
def compute_area_ratio(wl, curve, band_min, band_max):
    """
    Compute the ratio of the area under the curve in the band [band_min, band_max)
    to the total area under the curve.
    """
    total_area = np.trapezoid(curve, wl)
    band_mask = (wl >= band_min) & (wl <= band_max)
    band_area = np.trapezoid(curve[band_mask], wl[band_mask])
    return band_area / total_area if total_area > 0 else 0.0


# Compute area ratios for the Responsivity curves
uvc_area_ratio = compute_area_ratio(w, uvc, wl_min, cutoff_uvc_uvb)
uvb_area_ratio = compute_area_ratio(w, uvb, cutoff_uvc_uvb, cutoff_uvb_uva)
uva_area_ratio = compute_area_ratio(w, uva, cutoff_uvb_uva, wl_max)

# Correct Weighted Spectral Effectiveness
uvc_w_eff = uvc_w_eff * uvc_area_ratio
uvb_w_eff = uvb_w_eff * uvb_area_ratio
uva_w_eff = uva_w_eff * uva_area_ratio


# -----------------------------------------------------
# 3) In-band peaks
# -----------------------------------------------------
def find_in_band_peak(wl, y, band_min, band_max):
    """
    Return (peak_wl, peak_val) for the maximum of y where band_min <= wl <= band_max.
    """
    mask = (wl >= band_min) & (wl < band_max)
    if not np.any(mask):
        return None, None
    idx_max = np.argmax(y[mask])
    peak_val = y[mask][idx_max]
    peak_wl = wl[mask][idx_max]
    return peak_wl, peak_val


uvc_peak_wl, uvc_peak_val = find_in_band_peak(w, uvc_eff, wl_min, cutoff_uvc_uvb)
uvb_peak_wl, uvb_peak_val = find_in_band_peak(
    w, uvb_eff, cutoff_uvc_uvb, cutoff_uvb_uva
)
uva_peak_wl, uva_peak_val = find_in_band_peak(w, uva_eff, cutoff_uvb_uva, wl_max)


# -----------------------------------------------------
# 4) Plotting Helpers: Split in-band vs. out-of-band
#    We do it by creating arrays with NaN outside the band
#    so matplotlib doesn't draw lines across boundaries.
# -----------------------------------------------------
def band_split(wl, arr, band_min, band_max):
    """
    Return two arrays: in-band (NaN outside) and out-of-band (NaN inside),
    so we can plot them separately and avoid bridging lines across cutoff edges.
    """
    in_mask = (wl >= band_min) & (wl <= band_max)
    out_mask = (wl <= band_min) | (wl >= band_max)  # ~in_mask

    in_array = np.where(in_mask, arr, np.nan)
    out_array = np.where(out_mask, arr, np.nan)
    return in_array, out_array


# We'll also define a small function to plot the peak marker
def add_peak_marker(ax, peak_wl, peak_val, color):
    if peak_wl is not None and peak_val is not None:
        ax.scatter(peak_wl, peak_val, color=color, zorder=5)
        ax.text(
            peak_wl + 2,
            peak_val,
            f"{peak_val:.4g} @{peak_wl:.0f}nm",
            color=color,
            fontsize=9,
            ha="left",
            va="bottom",
        )


# -----------------------------------------------------
# 6) Create figure with 3 subplots
# -----------------------------------------------------
colors = {
    "UVC": "#4d4d4d",  # darker/grey
    "UVB": "#8a2be2",  # violet/purple
    "UVA": "#0b66f2",  # pleasing blue
}

fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 12), sharex=True)
plt.subplots_adjust(hspace=0.1)


# A helper to plot everything for a given subplot
def plot_subplot(
    ax,
    w,
    resp,
    eff,
    eff_curve,
    band_min,
    band_max,
    color,
    label,
    weighted_eff,
    peak_wl,
    peak_val,
):
    """
    Plots:
      - background (responsivity in linear scale, spectral eff in log scale)
      - in-band and out-of-band lines for 'eff_curve' (which is resp*eff)
      - vertical cutoffs
      - weighted efficiency text
      - peak marker
    """

    # Split in-band vs out-of-band
    main_in, main_out = band_split(w, eff_curve, band_min, band_max)
    p1 = ax.plot(
        w, main_out, color=color, alpha=0.4, linewidth=1, label=f"{label} - out"
    )
    p2 = ax.plot(w, main_in, color=color, alpha=1.0, linewidth=2, label=f"{label} - in")
    ax.set_ylim(0, 1.1 * np.max(eff_curve))

    # Background plots
    ax_resp = ax.twinx()
    ax_eff = ax.twinx()
    ax_eff.spines.right.set_position(("axes", 1.1))
    ax_eff.set_yscale("log", nonpositive="clip")

    p3 = ax_resp.plot(w, resp, color="grey", linestyle="-", alpha=0.5, label="Resp")
    p4 = ax_eff.plot(
        w,
        eff,
        color="grey",
        linestyle="--",
        alpha=0.6,
        label="SpecEff",
    )
    ax_resp.set_ylabel("Normalized Sensor Responsivity")
    ax_eff.set_ylabel("Relative Spectral Effectiveness")

    # Add cutoff lines
    ax.axvline(x=cutoff_uvc_uvb, color="black", linestyle="dotted", alpha=0.5)
    ax.axvline(x=cutoff_uvb_uva, color="black", linestyle="dotted", alpha=0.5)

    # Weighted Spectral Effectiveness text annotation
    text_str = f"Weighted Spectral Effectiveness (in band): {weighted_eff:.4g}"
    ax.text(
        0.00,
        1.0055,
        text_str,
        transform=ax.transAxes,
        color=color,
        fontsize=10,
        ha="left",
        va="bottom",
    )

    # Mark the peak
    add_peak_marker(ax, peak_wl, peak_val, color)

    # Final styling
    ax.legend(loc="upper right", handles=[p1[0], p2[0], p3[0], p4[0]])
    ax.set_ylabel(f"{label}")


# --------------------- Subplot for UVC --------------------------
plot_subplot(
    ax1,
    w,
    uvc,
    eff,
    uvc_eff,
    band_min=w[0],
    band_max=cutoff_uvc_uvb,
    color=colors["UVC"],
    label="Resp (UVC) * SpecEff",
    weighted_eff=uvc_w_eff,
    peak_wl=uvc_peak_wl,
    peak_val=uvc_peak_val,
)

# --------------------- Subplot for UVB --------------------------
plot_subplot(
    ax2,
    w,
    uvb,
    eff,
    uvb_eff,
    band_min=cutoff_uvc_uvb,
    band_max=cutoff_uvb_uva,
    color=colors["UVB"],
    label="Resp (UVB) * SpecEff",
    weighted_eff=uvb_w_eff,
    peak_wl=uvb_peak_wl,
    peak_val=uvb_peak_val,
)

# --------------------- Subplot for UVA --------------------------
plot_subplot(
    ax3,
    w,
    uva,
    eff,
    uva_eff,
    band_min=cutoff_uvb_uva,
    band_max=w[-1],
    color=colors["UVA"],
    label="Resp (UVA) * SpecEff",
    weighted_eff=uva_w_eff,
    peak_wl=uva_peak_wl,
    peak_val=uva_peak_val,
)

ax3.set_xlabel("Wavelength (nm)")

plt.tight_layout()
plt.show()

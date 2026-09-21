#!/usr/bin/env python3
"""Facts the branding is allowed to state, taken from the source rather than typed.

Two of them, and both exist because the old banner got them wrong:

  * The version. images/banner.png shipped a "v2.9" pill into the v3.0 release
    and nobody noticed, because it was a string in a drawing script. It is read
    from specter_i.h now.

  * The signal. The old banner drew a decorative gauge at a made-up 82% and a
    cartoon ghost. The house rule for this project is that the signature
    element is made of REAL product data - so the curve on the banner is the
    actual meter output for a walk up to a reader and away again, produced by
    compiling the app's own helpers (field_scale.c, emitter_classify.c,
    survey_verdict.c) through tools_gif_data.c. Same series the demo GIF runs on.

    python3 tools_brand_data.py     # print what it would hand the renderer
"""
import os
import re
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def version():
    """SPECTER_VERSION, straight out of the header the firmware compiles."""
    src = open(os.path.join(HERE, "specter_i.h")).read()
    m = re.search(r'#define\s+SPECTER_VERSION\s+"([^"]+)"', src)
    if not m:
        raise SystemExit("SPECTER_VERSION not found in specter_i.h")
    return m.group(1)


def approach():
    """The real meter trace for walking up to a reader, resting on it, leaving.

    Returns (shown, present, fingerprint) where `shown` is the 0..100 meter
    value per 100 ms UI tick, `present` is the latched detection flag, and
    `fingerprint` is the classifier's call on the cadence it measured.
    """
    helpers = [
        os.path.join(HERE, "helpers", f)
        for f in ("field_scale.c", "emitter_classify.c", "survey_verdict.c")
    ]
    with tempfile.TemporaryDirectory() as td:
        exe = os.path.join(td, "branddata")
        subprocess.run(
            ["cc", "-std=c11", "-Wall", "-Wextra", "-I", os.path.join(HERE, "helpers"),
             "-o", exe, os.path.join(HERE, "tools_gif_data.c")] + helpers,
            check=True,
        )
        out = subprocess.run([exe], check=True, capture_output=True, text=True).stdout

    shown, present, fp = [], [], None
    for line in out.splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        if line.startswith("FP "):
            f = line[3:].split("|")
            fp = {
                "klass": f[0], "blurb": f[1], "confidence": int(f[2]),
                "period_ms": int(f[3]), "burst_ms": int(f[4]),
                "jitter_ms": int(f[5]), "duty": int(f[6]),
                "timing_reliable": bool(int(f[7])),
            }
        elif line.startswith("SV "):
            continue
        else:
            f = line.split()
            shown.append(int(f[2]))
            present.append(int(f[4]))
    return shown, present, fp


def pulse_train(period_ms, burst_ms, columns, ms_per_col=8):
    """The carrier as the device's trace buffer would hold it: one column per
    slice, high while the burst is up. ms_per_col mirrors the detector's
    TRACE_SLICE_SAMPLES * SAMPLE_PERIOD_US."""
    per = max(1, round(period_ms / ms_per_col))
    bst = max(1, round(burst_ms / ms_per_col))
    return [1 if (i % per) < bst else 0 for i in range(columns)]


if __name__ == "__main__":
    shown, present, fp = approach()
    print(f"version      : {version()}")
    print(f"samples      : {len(shown)} at 100 ms  (peak {max(shown)})")
    print(f"detected at  : sample {present.index(1)}" if 1 in present else "never detected")
    print(f"fingerprint  : {fp['klass']} / {fp['blurb']} / conf {fp['confidence']}%")
    print(f"               period {fp['period_ms']}ms burst {fp['burst_ms']}ms duty {fp['duty']}%")
    print(f"curve        : {shown}")

#!/usr/bin/env python3
"""Facts the branding is allowed to state, taken from the source rather than typed.

Two of them, and both exist because the old banner got them wrong:

  * The version. images/banner.png shipped a "v2.9" pill into the v3.0 release
    and nobody noticed, because it was a string in a drawing script. It is read
    from specter_i.h now.

  * The signal. The old banner drew a decorative gauge at a made-up 82% and a
    cartoon ghost. The house rule for this project is that the signature
    element is made of REAL product data - so the waveform on the banner is
    read back out of a hardware capture in screenshots/, column by column.

    There used to be a second source here: a meter curve synthesised by
    compiling the app's own helpers through tools_gif_data.c, which fed the
    generated demo GIF. Both are gone as of 3.1. A renderer that reproduces
    the product is a second implementation of it, and the only thing this file
    is allowed to hand the banner now is a measurement.

    python3 tools_brand_data.py     # print what it would hand the renderer
"""
import os
import re

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))


def version():
    """SPECTER_VERSION, straight out of the header the firmware compiles."""
    src = open(os.path.join(HERE, "specter_i.h")).read()
    m = re.search(r'#define\s+SPECTER_VERSION\s+"([^"]+)"', src)
    if not m:
        raise SystemExit("SPECTER_VERSION not found in specter_i.h")
    return m.group(1)


def parse_defines(relpath, *names):
    """Pull #define integers straight out of a C source, so the renderer and the
    firmware cannot drift apart silently."""
    src = open(os.path.join(HERE, relpath)).read()
    out = []
    for n in names:
        m = re.search(r"^#define\s+" + re.escape(n) + r"\s+(-?\d+)", src, re.M)
        if not m:
            raise SystemExit(f"{n} not found in {relpath}")
        out.append(int(m.group(1)))
    return out


def carrier_from_capture(path=os.path.join("screenshots", "ss2.png")):
    """The raw 13.56 MHz carrier, read back out of a real hardware capture.

    This is the branding's signature element, and the reason it is a measurement
    rather than a drawing. views/fingerprint_view.c::draw_trace() marks every
    column with a dot at TRACE_HI (carrier up) or TRACE_LO (carrier down), and
    draws a full HI..LO vertical on every level CHANGE - so the middle row is
    inked by edges and by nothing else. Level is therefore an exact XOR chain
    down the mid row: no heuristic, no OCR, no guessing.

    Sampling the HI row directly does NOT work, because the edge verticals pass
    through it on falling edges too, which reads as high.

    Returns (bits, hi_row, lo_row).
    """
    full = os.path.join(HERE, path)
    im = Image.open(full).convert("RGB").resize((128, 64), Image.NEAREST)
    px = im.load()

    def ink(x, y):
        return px[x, y] == (0, 0, 0)

    # Derive the trace rows FROM THE CAPTURE rather than from the C: a capture
    # taken before a layout change sits a row or two off, and hardcoding the
    # current constant would silently read the wrong row.
    cnt = {y: sum(ink(x, y) for x in range(128)) for y in range(40, 64)}
    full_rows = [y for y in cnt if cnt[y] == 128]
    if not full_rows:
        raise SystemExit(
            f"{path}: no full-width divider found - is this a Fingerprint screen?"
        )
    div = max(full_rows)
    band = [y for y in range(div + 1, 64) if cnt[y] > 0]
    if not band:
        raise SystemExit(f"{path}: nothing drawn below the divider")
    hi, lo = min(band), max(band)
    mid = (hi + lo) // 2

    level = [ink(0, hi)]
    for i in range(1, 128):
        level.append(level[-1] ^ ink(i, mid))
    return [1 if v else 0 for v in level], hi, lo


if __name__ == "__main__":
    print(f"version      : {version()}")

    bits, hi, lo = carrier_from_capture()
    c_hi, c_lo = parse_defines("views/fingerprint_view.c", "TRACE_HI", "TRACE_LO")
    duty = 100.0 * sum(bits) / len(bits)
    edges = sum(1 for i in range(1, len(bits)) if bits[i] != bits[i - 1])
    print()
    print(f"carrier rows : capture {hi}/{lo}  vs source {c_hi}/{c_lo}")
    print(f"carrier duty : {duty:.1f}% over {len(bits)} columns, {edges} edges")
    print(f"first high   : column {bits.index(1) if 1 in bits else '-'}")
    print("".join(str(b) for b in bits))

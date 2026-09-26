#!/usr/bin/env python3
"""Facts the branding is allowed to state, taken from the source rather than typed.

The banner in this repository shipped a stale version number once already,
because it was a string in a drawing script. Nothing here is typed that could
be read instead:

  * The version comes from faraday_i.h, the header the firmware compiles.
  * The grade thresholds come from helpers/fdy_grade.h, the same #defines the
    grading engine branches on - so the scale drawn on the banner is the scale
    the app actually applies, and a threshold change cannot leave the artwork
    quietly describing an older build.
  * The device screen on the banner is a real capture out of screenshots/,
    pasted untouched. It is not a drawing of the UI: a drawing is a second
    implementation that can disagree with the firmware while looking
    convincing.

    python3 tools_brand_data.py     # print what it would hand the renderer
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))


def _read(*parts):
    with open(os.path.join(HERE, *parts)) as fh:
        return fh.read()


def version():
    """FARADAY_VERSION, straight out of the header the firmware compiles."""
    m = re.search(r'#define\s+FARADAY_VERSION\s+"([^"]+)"', _read("faraday_i.h"))
    if not m:
        raise SystemExit("FARADAY_VERSION not found in faraday_i.h")
    return m.group(1)


def parse_defines(relpath, *names):
    """Pull #define integers straight out of a C source, so the renderer and
    the firmware cannot drift apart silently."""
    src = _read(relpath)
    out = []
    for n in names:
        m = re.search(r"^#define\s+" + re.escape(n) + r"\s+(-?\d+)", src, re.M)
        if not m:
            raise SystemExit(f"{n} not found in {relpath}")
        out.append(int(m.group(1)))
    return out


def grade_scale():
    """[(letter, lower bound in dB), ...] best to worst, from fdy_grade.h.

    This is the banner's signature element: not ornament, and not a number
    anyone typed into a drawing - it is the instrument's own scale.
    """
    db = parse_defines(
        os.path.join("helpers", "fdy_grade.h"),
        "FDY_DB_APLUS",
        "FDY_DB_A",
        "FDY_DB_B",
        "FDY_DB_C",
        "FDY_DB_D",
    )
    return list(zip(("A+", "A", "B", "C", "D"), db))


def signal_margin_db():
    """The dB a carrier must clear before Faraday will lock a baseline."""
    (v,) = parse_defines(os.path.join("scenes", "faraday_scene_subghz.c"), "FDY_SIGNAL_MARGIN_DB")
    return v


def capture(*names):
    """The first of these captures that exists, as a path. No fallback to art."""
    for n in names:
        p = os.path.join(HERE, "screenshots", n)
        if os.path.exists(p):
            return p
    raise SystemExit(
        "no device capture available for the banner: run\n"
        "  python3 tools_screenshot.py --all\n"
        "The banner pastes a real screen; it does not draw one."
    )


if __name__ == "__main__":
    print(f"version        {version()}")
    print(f"signal margin  {signal_margin_db()} dB")
    print("grade scale")
    for letter, db in grade_scale():
        print(f"  {letter:>2}  {db:>3} dB and up")
    print(f"capture        {os.path.relpath(capture('verdict.png', 'baseline.png'), HERE)}")

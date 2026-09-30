#!/usr/bin/env python3
"""Generate 1-bit 10x10 Flipper icons for Specter from ASCII bitmaps.

'#' = foreground (black / on), anything else = background (white / off).
fbt thresholds PNGs to 1-bit where dark pixels become 'on'.

The app mark is ONE POLL CYCLE at SPECTER_FULL_SCALE_DUTY - the same object the
banner and the boot intro draw, and the same thing the Fingerprint screen
records. It used to be a little cartoon ghost, a leftover from the neon
identity the BACKLIGHT rebrand replaced everywhere else; the icon in the
Flipper's own app list was the one place it survived, which meant the first
thing anyone saw of Specter was the only piece of it that was still an
illustration. The house rule is that the signature element is a measurement.
"""
from PIL import Image
import os

OUT = os.path.join(os.path.dirname(__file__), "icons")
os.makedirs(OUT, exist_ok=True)

GLYPHS = {
    # App mark: one poll cycle. Low, burst, low - bleeding off both edges, so
    # it reads as a continuous carrier sampled rather than as a shape floating
    # in a box. The burst is 3 of 10 columns: SPECTER_FULL_SCALE_DUTY, the duty
    # a real terminal radiates and the value the whole meter is scaled against.
    "specter_10px": [
        "..........",
        "...####...",
        "...#..#...",
        "...#..#...",
        "...#..#...",
        "...#..#...",
        "...#..#...",
        "...#..#...",
        "####..####",
        "..........",
    ],
}


def render(name, rows):
    img = Image.new("1", (10, 10), 1)  # 1 = white background
    for y, row in enumerate(rows):
        for x, ch in enumerate(row[:10]):
            if ch == "#":
                img.putpixel((x, y), 0)  # 0 = black foreground
    path = os.path.join(OUT, name + ".png")
    img.save(path)
    return path


if __name__ == "__main__":
    for name, rows in GLYPHS.items():
        assert len(rows) == 10, f"{name} must have 10 rows"
        for r in rows:
            assert len(r) == 10, f"{name} row not 10 wide: {r!r}"
        p = render(name, rows)
        print("wrote", p)

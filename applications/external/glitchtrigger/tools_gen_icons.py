#!/usr/bin/env python3
"""Generate the 1-bit 10x10 Flipper app icon for Glitch Trigger from an ASCII
bitmap. '#' = foreground (black / on), anything else = background.
fbt thresholds PNGs to 1-bit where dark pixels become 'on'."""
from PIL import Image
import os

OUT = os.path.join(os.path.dirname(__file__), "icons")
os.makedirs(OUT, exist_ok=True)

GLYPHS = {
    # App mark: a bold fault/energy bolt - the instantaneous "glitch" that a
    # fault-injection pulse injects into a target's power rail.
    "glitch_10px": [
        ".....###..",
        "....###...",
        "...###....",
        "..######..",
        ".#######..",
        "....###...",
        "...###....",
        "..###.....",
        ".###......",
        "###.......",
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

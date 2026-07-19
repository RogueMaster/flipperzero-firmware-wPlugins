#!/usr/bin/env python3
"""Generate 1-bit 10x10 Flipper icons for Faraday from ASCII bitmaps.
'#' = foreground (black / on), anything else = background (white / off).
fbt thresholds PNGs to 1-bit where dark pixels become 'on'.
"""
from PIL import Image
import os

OUT = os.path.join(os.path.dirname(__file__), "icons")
os.makedirs(OUT, exist_ok=True)

GLYPHS = {
    # App mark: a sealed pouch with something solid shut inside it.
    "faraday_10px": [
        "..######..",
        ".########.",
        "#........#",
        "#..####..#",
        "#.######.#",
        "#.######.#",
        "#..####..#",
        "#........#",
        ".########.",
        "..........",
    ],
    # A radiating signal: antenna with carrier arcs either side.
    "wave_10px": [
        "....#.....",
        "..#.#.#...",
        ".#..#..#..",
        "#...#...#.",
        "#...#...#.",
        "#...#...#.",
        ".#..#..#..",
        "..#.#.#...",
        "....#.....",
        "..........",
    ],
    # A signal stopped dead by a shield wall.
    "block_10px": [
        "..#....##.",
        ".#.#...##.",
        "#..#...##.",
        "#..#...##.",
        "#..#...##.",
        "#..#...##.",
        "#..#...##.",
        ".#.#...##.",
        "..#....##.",
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
        print("wrote", render(name, rows))

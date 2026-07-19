#!/usr/bin/env python3
"""Generate 1-bit 10x10 Flipper icons for GhostTag from ASCII bitmaps.
'#' = foreground (black / on), anything else = background (white / off).
fbt thresholds PNGs to 1-bit where dark pixels become 'on'.
"""
from PIL import Image
import os

OUT = os.path.join(os.path.dirname(__file__), "icons")
os.makedirs(OUT, exist_ok=True)

GLYPHS = {
    # App / ghost mark
    "ghosttag_10px": [
        "..######..",
        ".########.",
        ".##.##.##.",
        ".########.",
        ".########.",
        ".########.",
        ".########.",
        ".########.",
        ".#.##.##.#",
        "..#..#..#.",
    ],
    "ghost_10px": [
        "..######..",
        ".########.",
        ".##.##.##.",
        ".########.",
        ".########.",
        ".########.",
        ".########.",
        ".########.",
        ".#.##.##.#",
        "..#..#..#.",
    ],
    # Apple / Find My (AirTag) - apple with leaf + bite
    "apple_10px": [
        ".....##...",
        "....##....",
        "...###....",
        "..#####.#.",
        ".#######..",
        ".########.",
        ".########.",
        ".#######..",
        "..######..",
        "...####...",
    ],
    # Tile - square with hole near top
    "tile_10px": [
        "..######..",
        ".###..###.",
        ".###..###.",
        ".########.",
        ".########.",
        ".########.",
        ".########.",
        ".########.",
        ".########.",
        "..######..",
    ],
    # Samsung SmartTag - rounded tag with keyring hole top-right
    "samsung_10px": [
        ".#####.##.",
        ".######.#.",
        ".##....##.",
        ".##....##.",
        ".########.",
        ".########.",
        ".########.",
        ".########.",
        ".########.",
        "..######..",
    ],
    # Warning triangle with exclamation
    "warning_10px": [
        "....#.....",
        "....#.....",
        "...###....",
        "...#.#....",
        "..##.##...",
        "..##.##...",
        ".###.###..",
        ".###v###..".replace("v", "#"),
        ".#######..",
        "..........",
    ],
    # Signal / radar dot
    "radar_10px": [
        "..######..",
        ".#......#.",
        "#..####..#",
        "#.#....#.#",
        "#.#.##.#.#",
        "#.#.##.#.#",
        "#.#....#.#",
        "#..####..#",
        ".#......#.",
        "..######..",
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
        p = render(name, rows)
        print("wrote", p)

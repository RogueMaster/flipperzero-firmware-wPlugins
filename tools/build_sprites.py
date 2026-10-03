#!/usr/bin/env python3
"""Build sprites.h (and a preview sheet) for Yulia's Cats.

Every sprite has three pixel states: black, white and transparent. They are
emitted as two XBM-order bitmaps (bit 0 = leftmost pixel), one per colour.

    python tools/build_sprites.py [--preview sheet.png]
"""

import argparse
import math
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent

T, W, B = 0, 1, 2  # transparent, white, black

# ---------------------------------------------------------------- Yulia

YULIA_W, YULIA_H = 42, 46


def build_yulia():
    g = [[T] * YULIA_W for _ in range(YULIA_H)]

    def put(x, y, v):
        if 0 <= x < YULIA_W and 0 <= y < YULIA_H:
            g[y][x] = v

    def ring(cx, cy, r, v):
        for y in range(YULIA_H):
            for x in range(YULIA_W):
                d = math.hypot(x - cx, y - cy)
                if r - 0.5 <= d < r + 0.5:
                    put(x, y, v)

    CX, CY, CHIN = 20.5, 20.0, 33

    # Shoulder-length bob: a round crown whose ends tuck in under the chin.
    for y in range(0, 38):
        if y <= 18:
            half = 18.5 * math.sqrt(max(0.0, 1 - ((y - 18) / 18.5) ** 2))
        elif y <= 29:
            half = 18.5 + 0.7 * math.sin((y - 18) / 2.5)
        else:
            half = 18.5 - ((y - 29) / 8.0) ** 2 * 5.5
        for x in range(YULIA_W):
            if abs(x - CX) <= half:
                put(x, y, B)

    # Small shoulders in a cozy sweater, and the neck.
    for y in range(37, YULIA_H):
        half = 9.5 + 7.5 * math.sqrt(min(1.0, (y - 37) / 6.0))
        for x in range(YULIA_W):
            d = abs(x - CX)
            if d <= half:
                put(x, y, B if d > half - 1 or y == 37 else W)
    for y in range(32, 39):
        for x in range(18, 24):
            put(x, y, W)
    for x in range(15, 27):  # ribbed collar
        put(x, 38, B)
        put(x, 40, B)
    for x in range(16, 26, 2):
        put(x, 39, B)

    # Heart-shaped face: full cheeks that taper to a little chin.
    def face_half(y):
        if y <= CY:
            return 12.5 * math.sqrt(max(0.0, 1 - ((y - CY) / 12.0) ** 2))
        u = (y - CY) / (CHIN - CY)
        return max(2.0, 12.5 * math.cos(u * math.pi / 2) ** 0.62)

    for y in range(8, CHIN + 1):
        half = face_half(y)
        for x in range(YULIA_W):
            if abs(x - CX) <= half:
                put(x, y, W)
    for x in range(18, 24):  # chin line over the neck
        if g[CHIN][x] != W or abs(x - CX) > 1.5:
            put(x, CHIN + 1 if abs(x - CX) <= 2 else CHIN, B)

    # Centre-parted fringe; the hairline makes the top of the heart.
    for x in range(YULIA_W):
        edge = 11.5 + 2.2 * math.cos((x - CX) / 12.5 * 2 * math.pi)
        for y in range(6, 16):
            if g[y][x] == W and y < edge:
                put(x, y, B)

    # Shine on the hair.
    for x in range(10, 24):
        y = round(4.5 - 2.0 * math.sin((x - 10) / 13 * math.pi))
        put(x, y, W)
    for x in range(25, 29):
        put(x, 4 + (x - 25) // 2, W)

    # Heart hair clip.
    for x, y in ((32, 9), (34, 9), (31, 10), (32, 10), (33, 10), (34, 10), (35, 10),
                 (32, 11), (33, 11), (34, 11), (33, 12)):
        put(x, y, W)

    # Big round glasses.
    for cx in (14.5, 26.5):
        ring(cx, 20, 5.5, B)
    for x in (20, 21):
        put(x, 19, B)

    # Blush.
    for x, y in ((11, 27), (13, 27), (12, 28), (28, 27), (30, 27), (29, 28)):
        put(x, y, B)

    return g


# ---------------------------------------------------------------- cats
#
# Legend: '#' outline, '.' fur, 's' stripe, 'e' dark eye, 'o' light,
# ' ' transparent.
#
# Nugget is an elderly domestic shorthair: dark tiger stripes, white belly
# and white socks.
# Baby is a chonky grey cat (dithered fur) with a little face and big
# bright eyes.

NUGGET_ART = {
    "sit": [
        " #       #   ",
        "###     ###  ",
        "####.#.####  ",
        "###########  ",
        "##.#####.##  ",
        "####...####  ",
        " ###.#.###   ",
        "  ##...##   #",
        " ###...### ##",
        " #.#...#.####",
        "###.....#### ",
        "#..#...#..#  ",
        "#..#...#..#  ",
        " #########   ",
    ],
    "walk_a": [
        "#             #  # ",
        "##           ######",
        " ##          #.##.#",
        "  ##############..#",
        "  #.##.##.######## ",
        "  ###########      ",
        "  #.........#      ",
        "  #.#######.#      ",
        "  #.#     #.#      ",
        "  ###     ###      ",
    ],
    "walk_b": [
        "              #  # ",
        "#            ######",
        "###          #.##.#",
        "  ##############..#",
        "  #.##.##.######## ",
        "  ###########      ",
        "  #.........#      ",
        " #..#######..#     ",
        " #.#       #.#     ",
        " ###       ###     ",
    ],
    "sleep": [
        " #  #         ",
        "############  ",
        "#..#..##.##.##",
        "#######.##.###",
        "##...........#",
        " ############ ",
    ],
}

BABY_ART = {
    "sit": [
        "     #     #        ",
        "    #.#####.#       ",
        "    #.......#       ",
        "    #ooo.ooo#       ",
        "    #oeo.oeo#       ",
        "    #...#...#       ",
        "   ##.......##      ",
        "  #...........#     ",
        " #.............#  ##",
        "#...............##.#",
        "#...............##.#",
        "#....#.....#....#.# ",
        "#....#.....#....##  ",
        "#....#.....#....#   ",
        " ###############    ",
    ],
    "walk_a": [
        "##                #   # ",
        "#.#              #.###.#",
        " #.#             #oo.oo#",
        "  #.##############oe.oe#",
        "  #..............#..#..#",
        "  #...............##### ",
        "  #.................#   ",
        "  #.................#   ",
        "  #.................#   ",
        "   #...............#    ",
        "   #..####...####..#    ",
        "   #.#    #.#    #.#    ",
        "   ###    ###    ###    ",
    ],
    "walk_b": [
        "                  #   # ",
        "###              #.###.#",
        "#..#             #oo.oo#",
        " ##.##############oe.oe#",
        "  #..............#..#..#",
        "  #...............##### ",
        "  #.................#   ",
        "  #.................#   ",
        "  #.................#   ",
        "   #...............#    ",
        "   #..####...####..#    ",
        "  #.#     #.#     #.#   ",
        "  ###     ###     ###   ",
    ],
    "sleep": [
        "  #   #              ",
        " #.###.###########   ",
        " #.....#..........## ",
        "#.ee.ee.#...........#",
        "#...#...............#",
        "#...................#",
        "#....###########....#",
        " ####...........#### ",
        "     ###########     ",
    ],
}

NUGGET = {"#": B, ".": W, "s": B, "e": B, "o": W, " ": T}
GREY = "grey"
BABY = {"#": B, ".": GREY, "s": B, "e": B, "o": W, " ": T}

# ---------------------------------------------------------------- props

PLAIN = {"#": B, ".": W, " ": T}

PROPS = {
    "heart": [
        " ## ## ",
        "#######",
        "#######",
        " ##### ",
        "  ###  ",
        "   #   ",
    ],
    "heart_small": [
        "# #",
        "###",
        " # ",
    ],
    "bowl": [
        "#############",
        "#...........#",
        " #.........# ",
        " #.........# ",
        "  #########  ",
    ],
    "bowl_full": [
        "    #####    ",
        "  ##.#.#.##  ",
        "#############",
        "#...........#",
        " #.........# ",
        " #.........# ",
        "  #########  ",
    ],
    "yarn": [
        "  ###  ",
        " #.#.# ",
        "#.#.#.#",
        "##.#.##",
        "#.#.#.#",
        " #.#.# ",
        "  ###  ",
    ],
    "mug": [
        "#######  ",
        "#.....###",
        "#.....#.#",
        "#.....#.#",
        "#.....## ",
        "#.....#  ",
        " #####   ",
    ],
}

# ---------------------------------------------------------------- output


def from_ascii(rows, palette):
    width = max(len(r) for r in rows)
    grid = [[palette[ch] for ch in row.ljust(width)] for row in rows]
    for y, row in enumerate(grid):
        for x, px in enumerate(row):
            if px == GREY:  # light brick dither
                row[x] = B if y % 2 == 0 and x % 2 == (y // 2) % 2 else W
    return grid


def flipped(grid):
    return [list(reversed(row)) for row in grid]


def xbm(grid, value):
    out = []
    for row in grid:
        for x0 in range(0, len(row), 8):
            byte = 0
            for bit, px in enumerate(row[x0 : x0 + 8]):
                if px == value:
                    byte |= 1 << bit
            out.append(byte)
    return out


def c_array(name, data):
    body = ", ".join(f"0x{b:02X}" for b in data)
    return f"static const uint8_t {name}[] = {{{body}}};\n"


def collect():
    sprites = {"yulia": build_yulia()}
    for cat, art, palette in (("nugget", NUGGET_ART, NUGGET), ("baby", BABY_ART, BABY)):
        for pose, rows in art.items():
            grid = from_ascii(rows, palette)
            sprites[f"{cat}_{pose}"] = grid
            if pose.startswith("walk"):
                sprites[f"{cat}_{pose}_l"] = flipped(grid)
    for name, rows in PROPS.items():
        sprites[name] = from_ascii(rows, PLAIN)
    return sprites


def write_header(sprites, path):
    out = [
        "// Generated by tools/build_sprites.py - do not edit by hand.\n",
        "#pragma once\n\n#include <stdint.h>\n\n",
        "typedef struct {\n    uint8_t w;\n    uint8_t h;\n"
        "    const uint8_t* black;\n    const uint8_t* white;\n} Sprite;\n\n",
    ]
    for name, grid in sprites.items():
        out.append(c_array(f"spr_{name}_black", xbm(grid, B)))
        out.append(c_array(f"spr_{name}_white", xbm(grid, W)))
        out.append(
            f"static const Sprite spr_{name} = "
            f"{{{len(grid[0])}, {len(grid)}, spr_{name}_black, spr_{name}_white}};\n\n"
        )
    path.write_text("".join(out))


def yulia_demo(grid):
    """Yulia with the eyes and smile the app draws on top, for previews."""
    g = [row[:] for row in grid]
    for cx in (14, 26):
        for y in range(18, 22):
            for x in range(cx, cx + 2):
                g[y][x] = B
    for x, y in ((18, 29), (23, 29), (19, 30), (20, 30), (21, 30), (22, 30)):
        g[y][x] = B
    return g


def write_preview(sprites, path, scale=6):
    sprites = {"demo": yulia_demo(sprites["yulia"]), **sprites}
    pad = 4
    width = sum(len(g[0]) + pad for g in sprites.values()) + pad
    height = max(len(g) for g in sprites.values()) + pad * 2
    img = Image.new("RGB", (width, height), (255, 140, 40))
    x0 = pad
    colours = {W: (255, 235, 200), B: (0, 0, 0)}
    for grid in sprites.values():
        for y, row in enumerate(grid):
            for x, px in enumerate(row):
                if px != T:
                    img.putpixel((x0 + x, pad + y), colours[px])
        x0 += len(grid[0]) + pad
    img.resize((width * scale, height * scale), Image.NEAREST).save(path)


def write_icon(path):
    rows = [
        "#.......#.",
        "##.....##.",
        "#########.",
        "#.#####.#.",
        "#########.",
        "####.####.",
        "###.#.###.",
        ".#######..",
        "..........",
        "..........",
    ]
    img = Image.new("1", (10, 10), 1)
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch == "#":
                img.putpixel((x, y + 1 if y < 9 else y), 0)
    img.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    args = ap.parse_args()
    sprites = collect()
    write_header(sprites, ROOT / "sprites.h")
    write_icon(ROOT / "icon.png")
    if args.preview:
        write_preview(sprites, args.preview)
    print(f"{len(sprites)} sprites -> sprites.h")


if __name__ == "__main__":
    main()

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

# Yulia is drawn in layers so her look can be changed in the wardrobe:
# sweater, then face, then hair, then glasses. Eyes and mouth are drawn by
# the app on top.

YULIA_W, YULIA_H = 42, 54
OFF = 8  # room above the head for buns
CX, CY, CHIN = 20.5, 20.0 + OFF, 33 + OFF

HAIR_STYLES = ["Bob", "Long", "Bun", "Buns"]
HAIR_COLORS = ["Black", "Brown", "Blonde"]
GLASSES = ["Round", "Square", "Kitty", "Bold"]
SWEATERS = ["Cozy", "Stripes", "Heart", "Dark"]


def blank():
    return [[T] * YULIA_W for _ in range(YULIA_H)]


def cells():
    for y in range(YULIA_H):
        for x in range(YULIA_W):
            yield x, y


def neighbours(x, y):
    return ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1))


def face_half(y):
    if y <= CY:
        return 12.5 * math.sqrt(max(0.0, 1 - ((y - CY) / 12.0) ** 2))
    u = (y - CY) / (CHIN - CY)
    return max(2.0, 12.5 * math.cos(u * math.pi / 2) ** 0.62)


# Heart-shaped face: full cheeks that taper to a little chin.
FACE = {(x, y) for x, y in cells() if OFF + 8 <= y <= CHIN and abs(x - CX) <= face_half(y)}
NECK = {(x, y) for x, y in cells() if 18 <= x <= 23 and CHIN - 1 <= y <= OFF + 38}


def sweater_half(y):
    return 9.5 + 7.5 * math.sqrt(min(1.0, (y - OFF - 37) / 6.0))


BODY = {(x, y) for x, y in cells() if y >= OFF + 37 and abs(x - CX) <= sweater_half(y)}


def hairline(x):
    """Centre-parted fringe; the hairline makes the top of the heart."""
    return OFF + 11.5 + 2.2 * math.cos((x - CX) / 12.5 * 2 * math.pi)


def hair_shape(style):
    def inside(x, y):
        yy, dx = y - OFF, abs(x - CX)
        if style == "Bun" and math.hypot(x - CX, yy + 2) <= 6.2:
            return True
        if style == "Buns" and math.hypot(dx - 13, yy - 1) <= 5.6:
            return True
        if yy < 0:
            return False
        if yy <= 18:
            return dx <= 18.5 * math.sqrt(max(0.0, 1 - ((yy - 18) / 18.5) ** 2))
        if style == "Bob":
            if yy <= 29:
                return dx <= 18.5 + 0.7 * math.sin((yy - 18) / 2.5)
            return yy <= 37 and dx <= 18.5 - ((yy - 29) / 8.0) ** 2 * 5.5
        if style == "Long":
            return dx <= 18.5 + 0.9 * math.sin((yy - 18) / 2.5)
        # Hair up: just a little hair tucked behind the ears.
        return yy <= 27 and dx <= 18.5 - ((yy - 18) / 9.0) ** 2 * 7.0

    mask = set()
    for x, y in cells():
        if not inside(x, y):
            continue
        if (x, y) in FACE:
            if y >= hairline(x):
                continue
        elif (x, y) in NECK:
            continue
        elif (x, y) in BODY or y >= OFF + 37:
            # Long hair falls in front of the shoulders, not the chest.
            if not (style == "Long" and abs(x - CX) >= 11):
                continue
        mask.add((x, y))
    return mask


def build_hair(style, color):
    mask = hair_shape(style)
    g = blank()
    for x, y in mask:
        edge = any(n not in mask for n in neighbours(x, y))
        if color == "Black" or edge:
            g[y][x] = B
        elif color == "Brown":
            g[y][x] = B if (x + y) % 2 else W
        else:
            g[y][x] = W

    accent = B if color == "Blonde" else W
    shine = [(x, round(OFF + 4.5 - 2.0 * math.sin((x - 10) / 13 * math.pi))) for x in range(10, 24)]
    shine += [(x, OFF + 4 + (x - 25) // 2) for x in range(25, 29)]
    clip = [(32, 9), (34, 9), (31, 10), (32, 10), (33, 10), (34, 10), (35, 10),
            (32, 11), (33, 11), (34, 11), (33, 12)]
    for x, y in shine + [(x, y + OFF) for x, y in clip]:
        if (x, y) in mask:
            g[y][x] = accent
    if color == "Blonde":  # a few strands so light hair still reads as hair
        for x, y in mask:
            if abs(x - CX) in (15.5, 16.5) and y % 3 and OFF + 14 <= y and g[y][x] == W:
                if abs(x - CX) == 15.5 + (y // 6) % 2:
                    g[y][x] = B
    return g


def build_face():
    g = blank()
    for x, y in cells():
        if (x, y) in FACE:
            g[y][x] = W
        elif any(n in FACE for n in neighbours(x, y)) and (x, y) not in NECK:
            g[y][x] = B  # outline, for the looks where hair doesn't frame it
    for x in range(19, 23):  # chin line over the neck
        g[CHIN + 1][x] = B
    for x, y in ((11, 27), (13, 27), (12, 28), (28, 27), (30, 27), (29, 28)):  # blush
        g[y + OFF][x] = B
    return g


def build_sweater(kind):
    g = blank()
    for x, y in NECK:
        g[y][x] = W
    for y in range(CHIN + 1, OFF + 38):
        g[y][17] = g[y][24] = B
    dark = kind == "Dark"
    for x, y in BODY:
        edge = abs(x - CX) > sweater_half(y) - 1 or y == OFF + 37
        g[y][x] = B if edge or dark else W
    for x, y in NECK:
        if y >= OFF + 37:
            g[y][x] = W
    rib, gap = (W, B) if dark else (B, W)
    for x in range(15, 27):  # ribbed collar
        g[OFF + 38][x] = rib
        g[OFF + 39][x] = rib if x % 2 == 0 else gap
        g[OFF + 40][x] = rib
    if kind == "Stripes":
        for y in (OFF + 42, OFF + 44):
            for x in range(YULIA_W):
                if (x, y) in BODY and g[y][x] == W:
                    g[y][x] = B
    if kind == "Heart":
        for dy, row in enumerate((" # # ", "#####", " ### ", "  #  ")):
            for dx, ch in enumerate(row):
                if ch == "#":
                    g[OFF + 42 + dy][18 + dx] = B
    return g


def build_glasses(kind):
    g = blank()
    cy = OFF + 20
    for cx in (14.5, 26.5):
        for x, y in cells():
            d = math.hypot(x - cx, y - cy)
            if kind == "Round" and 5.0 <= d < 6.0:
                g[y][x] = B
            elif kind == "Bold" and 4.2 <= d < 6.0:
                g[y][x] = B
            elif kind == "Square":
                ax, ay = abs(x - cx), abs(y - cy)
                if max(ax, ay) == 4.5 + (ay > ax) * 0.5 and not (ax == 4.5 and ay == 5):
                    g[y][x] = B
            elif kind == "Kitty":
                if 5.0 <= d < 6.0 and y >= cy - 2:
                    g[y][x] = B
                elif y == cy - 3 and abs(x - cx) <= 5.5:
                    g[y][x] = B
        if kind == "Kitty":  # upswept outer corners
            side = -1 if cx < CX else 1
            tip = int(cx + side * 5.5)
            g[cy - 4][tip] = g[cy - 4][tip + side] = g[cy - 5][tip + side] = B
    for x in (20, 21):
        g[cy - 1][x] = B
    return g


def yulia_layers():
    layers = {"yulia_face": build_face()}
    for style in HAIR_STYLES:
        for color in HAIR_COLORS:
            layers[f"hair_{style.lower()}_{color.lower()}"] = build_hair(style, color)
    for i, kind in enumerate(GLASSES):
        layers[f"glasses_{i}"] = build_glasses(kind)
    for i, kind in enumerate(SWEATERS):
        layers[f"sweater_{i}"] = build_sweater(kind)
    return layers


def compose(layers, style, color, glasses, sweater, features=True):
    g = blank()
    names = [f"sweater_{sweater}", "yulia_face",
             f"hair_{HAIR_STYLES[style].lower()}_{HAIR_COLORS[color].lower()}",
             f"glasses_{glasses}"]
    for name in names:
        for x, y in cells():
            if layers[name][y][x] != T:
                g[y][x] = layers[name][y][x]
    if features:  # the eyes and smile the app draws on top
        for cx in (14, 26):
            for y in range(OFF + 18, OFF + 22):
                for x in range(cx, cx + 2):
                    g[y][x] = B
        for x, y in ((18, 29), (23, 29), (19, 30), (20, 30), (21, 30), (22, 30)):
            g[y + OFF][x] = B
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
    "note": [
        "  ## ",
        "  # #",
        "  #  ",
        "  #  ",
        "###  ",
        "###  ",
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
    sprites = dict(yulia_layers())
    for cat, art, palette in (("nugget", NUGGET_ART, NUGGET), ("baby", BABY_ART, BABY)):
        for pose, rows in art.items():
            grid = from_ascii(rows, palette)
            sprites[f"{cat}_{pose}"] = grid
            if pose.startswith("walk"):
                sprites[f"{cat}_{pose}_l"] = flipped(grid)
    for name, rows in PROPS.items():
        sprites[name] = from_ascii(rows, PLAIN)
    return sprites


def c_names(name, values):
    body = ", ".join(f'"{v}"' for v in values)
    return f"static const char* const {name}[] = {{{body}}};\n"


def wardrobe_tables():
    out = [
        f"#define HAIR_STYLE_COUNT {len(HAIR_STYLES)}\n",
        f"#define HAIR_COLOR_COUNT {len(HAIR_COLORS)}\n",
        f"#define GLASSES_COUNT    {len(GLASSES)}\n",
        f"#define SWEATER_COUNT    {len(SWEATERS)}\n\n",
        c_names("hair_style_names", HAIR_STYLES),
        c_names("hair_color_names", HAIR_COLORS),
        c_names("glasses_names", GLASSES),
        c_names("sweater_names", SWEATERS),
        "\nstatic const Sprite* const yulia_hair[HAIR_STYLE_COUNT][HAIR_COLOR_COUNT] = {\n",
    ]
    for style in HAIR_STYLES:
        row = ", ".join(f"&spr_hair_{style.lower()}_{c.lower()}" for c in HAIR_COLORS)
        out.append(f"    {{{row}}},\n")
    out.append("};\n")
    row = ", ".join(f"&spr_glasses_{i}" for i in range(len(GLASSES)))
    out.append(f"static const Sprite* const yulia_glasses[GLASSES_COUNT] = {{{row}}};\n")
    row = ", ".join(f"&spr_sweater_{i}" for i in range(len(SWEATERS)))
    out.append(f"static const Sprite* const yulia_sweater[SWEATER_COUNT] = {{{row}}};\n")
    return out


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
    out += wardrobe_tables()
    path.write_text("".join(out))


def write_preview(sprites, path, scale=4):
    """A sheet of Yulia's looks, then every non-layer sprite."""
    looks = [compose(sprites, st, co, (st + co) % len(GLASSES), (st + co) % len(SWEATERS))
             for co in range(len(HAIR_COLORS)) for st in range(len(HAIR_STYLES))]
    looks += [compose(sprites, 0, 0, gl, gl) for gl in range(len(GLASSES))]
    layer = ("yulia_", "hair_", "glasses_", "sweater_")
    rest = [g for name, g in sprites.items() if not name.startswith(layer)]
    pad, per_row = 4, 8
    width = per_row * (YULIA_W + pad) + pad
    height = 3 * (YULIA_H + pad) + pad
    img = Image.new("RGB", (width, height), (255, 140, 40))
    colours = {W: (255, 235, 200), B: (0, 0, 0)}

    def blit(grid, x0, y0):
        for y, row in enumerate(grid):
            for x, px in enumerate(row):
                if px != T and x0 + x < width and y0 + y < height:
                    img.putpixel((x0 + x, y0 + y), colours[px])

    for i, grid in enumerate(looks):
        blit(grid, pad + (i % per_row) * (YULIA_W + pad), pad + (i // per_row) * (YULIA_H + pad))
    x0 = pad
    for grid in rest:
        blit(grid, x0, pad + 2 * (YULIA_H + pad))
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

#!/usr/bin/env python3
"""Build sprites.h (and a preview sheet) for Yulia's Cats.

Every sprite has three pixel states: black, white and transparent. They are
emitted as two XBM-order bitmaps (bit 0 = leftmost pixel), one per colour.

    python tools/build_sprites.py [--preview sheet.png]
"""

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent

T, W, B = 0, 1, 2  # transparent, white, black

# ---------------------------------------------------------------- Yulia

# Yulia is drawn in layers so her look can be changed in the wardrobe:
# sweater, then face, then hair, then glasses. Eyes and mouth are drawn by
# the app on top.

YULIA_W, YULIA_H = 42, 54
OFF = 8  # room above the head for buns
CX, CY, CHIN = 20.5, 20.0 + OFF, 33 + OFF

# New entries go on the end of each list: saves store the position.
HAIR_STYLES = ["Bob", "Long", "Bun", "Buns", "Pixie", "Pony", "Braids", "Curly", "Bangs"]
HAIR_COLORS = ["Black", "Brown", "Blonde"]
GLASSES = ["Round", "Square", "Kitty", "Bold", "None", "Tiny", "Oval", "Heart", "Shades"]
SWEATERS = ["Cozy", "Stripes", "Heart", "Dark", "Dots", "Zigzag", "Cat", "Star", "Checks"]


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


# Where each style's fringe is swept from: the hairline is a round arc that
# is highest at this x and curves down towards the temples.
HAIR_PARTS = {"Bob": CX, "Long": CX - 5, "Bun": CX + 5, "Buns": CX,
              "Pixie": CX - 7, "Pony": CX + 5, "Braids": CX, "Curly": CX}


def hairline(x, style):
    """A rounded hairline, biased to the centre, left or right by style."""
    if style == "Bangs":  # a blunt fringe straight across the forehead
        return OFF + 13.5
    u = min(1.0, abs(x - HAIR_PARTS[style]) / 16.0)
    return min(OFF + 14.5, OFF + 9.5 + 7.0 * (1 - math.sqrt(1 - u * u)))


def hair_shape(style):
    def inside(x, y):
        yy, dx = y - OFF, abs(x - CX)
        if style == "Bun" and math.hypot(x - CX, yy + 2) <= 6.2:
            return True
        if style == "Buns" and math.hypot(dx - 13, yy - 1) <= 5.6:
            return True
        if style == "Curly":
            # A big cloud of curls: the same outline as a bob, with a wavy edge.
            if yy <= 18:
                wave = 1.5 * math.sin(math.atan2(yy - 18, x - CX) * 10)
                return math.hypot(x - CX, yy - 18) <= 18.5 + wave
            width = 18.6 + 1.5 * math.sin(yy * 1.25)
            if yy > 25:
                width -= ((yy - 25) / 7.0) ** 2 * 5.0
            return yy <= 32 and dx <= width
        if style == "Pony":
            # The tail swings out to one side from a tie high on the head.
            tail_x = CX + 16.5 + 1.5 * math.sin(yy / 5.0)
            if 8 <= yy <= 32 and abs(x - tail_x) <= 2.6 - max(0, yy - 24) * 0.2:
                return True
        if style == "Braids":
            # Two plaits in front of the shoulders, pinched in every few rows.
            if 18 < yy and abs(dx - 15.0) <= 2.4 + 0.9 * abs(math.sin(yy * math.pi / 4)):
                return True
        if yy < 0:
            return False
        if yy <= 18:
            return dx <= 18.5 * math.sqrt(max(0.0, 1 - ((yy - 18) / 18.5) ** 2))
        if style == "Pixie":
            return yy <= 23 and dx <= 18.5 - ((yy - 18) / 5.0) ** 2 * 6.0
        if style in ("Bob", "Bangs"):
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
            if y >= hairline(x, style):
                continue
        elif (x, y) in NECK:
            continue
        elif (x, y) in BODY or y >= OFF + 37:
            # Long hair falls in front of the shoulders, not the chest.
            if not (style in ("Long", "Braids") and abs(x - CX) >= 11):
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
    extra = []
    if style == "Pony":  # the hair tie
        extra = [(x, OFF + 10) for x in range(34, YULIA_W)]
    elif style == "Braids":  # one line across each plait where it is pinched in
        extra = [(x, y) for x, y in mask
                 if y - OFF > 20 and (y - OFF) % 4 == 0 and abs(x - CX) >= 12]
    elif style == "Curly":  # a scatter of little curls
        extra = [(x, y) for x, y in mask
                 if (x * 7 + y * 13) % 29 == 0 and y > OFF + 6
                 and all(n in mask for n in neighbours(x, y))]
    for x, y in shine + [(x, y + OFF) for x, y in clip] + extra:
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
    emblems = {
        "Heart": (" # # ", "#####", " ### ", "  #  "),
        "Cat": ("#   #", "#####", "# # #", " ### "),
        "Star": ("  #  ", "#####", " ### ", " # # "),
    }
    for dy, row in enumerate(emblems.get(kind, ())):
        for dx, ch in enumerate(row):
            if ch == "#":
                g[OFF + 42 + dy][18 + dx] = B
    for x, y in BODY:
        if g[y][x] != W or y < OFF + 42:
            continue
        if kind == "Dots" and (x % 4, y - OFF) in ((1, 42), (3, 44)):
            g[y][x] = B
        elif kind == "Zigzag" and y - OFF == 42 + (0, 1, 2, 1)[x % 4]:
            g[y][x] = B
        elif kind == "Checks" and (x // 2 + y // 2) % 2 == 0:
            g[y][x] = B
    return g


def lens_inside(kind, dx, dy):
    """Whether a point is inside a lens of the newer frame shapes."""
    if kind == "Oval":
        return (dx / 5.9) ** 2 + (dy / 4.0) ** 2 <= 1
    if kind == "Heart":
        u, v = dx / 4.9, -(dy - 1) / 4.9
        return (u * u + v * v - 1) ** 3 - u * u * v ** 3 <= 0
    return math.hypot(dx, dy) < {"Tiny": 4.0, "Shades": 6.0}[kind]


def build_glasses(kind):
    g = blank()
    cy = OFF + 20
    if kind == "None":
        return g
    if kind in ("Tiny", "Oval", "Heart", "Shades"):
        for cx in (14.5, 26.5):
            lens = {(x, y) for x, y in cells() if lens_inside(kind, x - cx, y - cy)}
            for x, y in lens:
                if kind == "Shades" or any(n not in lens for n in neighbours(x, y)):
                    g[y][x] = B
            if kind == "Shades":  # a glint on each dark lens
                g[cy - 3][int(cx - 2)] = g[cy - 2][int(cx - 3)] = W
        for x in range(19, 23) if kind == "Tiny" else (20, 21):
            g[cy - 1][x] = B
        return g
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
    "treat": [
        " ###  #",
        "#####.#",
        " ###  #",
    ],
    "book": [
        " ######### ######### ",
        "#.........#.........#",
        "#.#######.#.#######.#",
        "#.........#.........#",
        "#.#######.#.#####...#",
        "#.........#.........#",
        "#.#####...#.#######.#",
        "#.........#.........#",
        " ######### ######### ",
    ],
    "pad": [
        "###############",
        "#.............#",
        "#..#.....#....#",
        "#..##...##....#",
        "#..#######....#",
        "#..#.#.#.#....#",
        "#..#######....#",
        "#...#####.....#",
        "#.............#",
        "###############",
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

# ---------------------------------------------------------------- extras


def build_keyboard():
    """Yulia's keyboard: white keys four pixels wide with black keys on top."""
    w, h = 37, 9
    g = [[W] * w for _ in range(h)]
    for x in range(w):
        g[0][x] = g[h - 1][x] = B
    for y in range(h):
        for x in range(0, w, 4):
            g[y][x] = B
    for k in range(1, w // 4):
        if k % 7 not in (0, 3):  # no black key between E-F and B-C
            for y in range(1, 5):
                for x in range(k * 4 - 1, k * 4 + 2):
                    g[y][x] = B
    return g


# Things Yulia says to the cats. The Flipper's fonts have no Cyrillic, so each
# phrase is rendered to a bitmap here. (key, Russian, English)
PHRASES = [
    ("PIGGY", "Ты моя хрюшка!", "You're my piggy!"),
    ("PIGLET", "Ах, поросёнок!", "Oh, you piglet!"),
    ("GOOD_KITTY", "Хороший котик!", "Good kitty!"),
    ("COME", "Иди ко мне!", "Come to me!"),
    ("OLD_FELLOW", "Мой старичок", "My old fellow"),
    ("SWEET", "Сладкий мой", "My sweet boy"),
    ("LITTLE_GIRL", "Моя малышка", "My little girl"),
    ("HUNGRY", "Кушать хочешь?", "Are you hungry?"),
    ("GOOD_NIGHT", "Спокойной ночи", "Good night"),
    ("GOOD_MORNING", "Доброе утро!", "Good morning!"),
    ("PIGGY_SHORT", "Хрюшка!", "Piggy!"),
]
RU_FONT = "/System/Library/Fonts/Monaco.ttf"
RU_SIZE = 9


def build_phrase(text):
    font = ImageFont.truetype(RU_FONT, RU_SIZE)
    img = Image.new("1", (160, 24), 1)
    draw = ImageDraw.Draw(img)
    draw.fontmode = "1"
    draw.text((2, 4), text, font=font, fill=0)
    box = img.convert("L").point(lambda v: 255 - v).getbbox()
    img = img.crop(box)
    return [[B if img.getpixel((x, y)) == 0 else T for x in range(img.width)]
            for y in range(img.height)]


def phrase_tables():
    out = ["\nenum {\n"]
    out += [f"    RU_{key},\n" for key, _, _ in PHRASES]
    out.append("    RU_COUNT,\n};\n")
    row = ", ".join(f"&spr_ru_{i}" for i in range(len(PHRASES)))
    out.append(f"static const Sprite* const ru_sprites[RU_COUNT] = {{{row}}};\n")
    out.append(c_names("ru_english", [en for _, _, en in PHRASES]))
    return out


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
    sprites["keyboard"] = build_keyboard()
    for i, (_, text, _) in enumerate(PHRASES):
        sprites[f"ru_{i}"] = build_phrase(text)
    return sprites


def c_names(name, values):
    body = ", ".join(f'"{v}"' for v in values)
    return f"static const char* const {name}[] = {{{body}}};\n"


def wardrobe_tables():
    out = [
        f"#define HAIR_STYLE_COUNT {len(HAIR_STYLES)}\n",
        f"#define HAIR_COLOR_COUNT {len(HAIR_COLORS)}\n",
        f"#define GLASSES_COUNT    {len(GLASSES)}\n",
        f"#define GLASSES_SHADES   {GLASSES.index('Shades')}\n",
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
    out += phrase_tables()
    path.write_text("".join(out))


def write_preview(sprites, path, scale=4):
    """A sheet of Yulia's looks, then every non-layer sprite."""
    looks = [compose(sprites, st, co, (st + co) % len(GLASSES), (st + co) % len(SWEATERS))
             for co in range(len(HAIR_COLORS)) for st in range(len(HAIR_STYLES))]
    looks += [compose(sprites, 0, 0, gl, gl) for gl in range(len(GLASSES))]
    layer = ("yulia_", "hair_", "glasses_", "sweater_", "ru_")
    rest = [g for name, g in sprites.items() if not name.startswith(layer)]
    pad, per_row = 4, len(HAIR_STYLES)
    look_rows = -(-len(looks) // per_row)
    width = max(per_row * (YULIA_W + pad) + pad, sum(len(g[0]) + pad for g in rest) + pad)
    height = (look_rows + 1) * (YULIA_H + pad) + pad
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
        blit(grid, x0, pad + look_rows * (YULIA_H + pad))
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

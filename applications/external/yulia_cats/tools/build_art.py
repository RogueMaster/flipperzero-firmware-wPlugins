#!/usr/bin/env python3
"""Build art.h (and a preview sheet) for the View screens of Yulia's Cats.

Each picture is a full 128x64 screen, drawn here from shapes rather than by
hand: smooth outlines, flat fills and a few dither tones, in the manner of
the Flipper's own dolphin artwork.

    python tools/build_art.py [--preview sheet.png]
"""

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
W, H = 128, 64

# Tones: what a filled area looks like. "paper" is the unlit screen.
TONES = {
    "paper": lambda x, y: False,
    "ink": lambda x, y: True,
    "dark": lambda x, y: (x + y) % 2 == 0,
    "mid": lambda x, y: x % 2 == 0 and y % 2 == 0,
    "light": lambda x, y: (x % 4 == 0 and y % 4 == 0) or (x % 4 == 2 and y % 4 == 2),
}


def spline(points, closed=True, steps=10):
    """A smooth curve through the points (Catmull-Rom)."""
    pts = list(points)
    n = len(pts)
    out = []
    last = n if closed else n - 1
    for i in range(last):
        p0 = pts[(i - 1) % n] if closed or i > 0 else pts[0]
        p1 = pts[i]
        p2 = pts[(i + 1) % n]
        p3 = pts[(i + 2) % n] if closed or i + 2 < n else pts[n - 1]
        for s in range(steps):
            t = s / steps
            t2, t3 = t * t, t * t * t
            out.append(
                tuple(
                    0.5
                    * (
                        (2 * p1[k])
                        + (-p0[k] + p2[k]) * t
                        + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2
                        + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3
                    )
                    for k in (0, 1)
                )
            )
    if not closed:
        out.append(pts[-1])
    return out


def oval(cx, cy, rx, ry, steps=48):
    return [
        (
            cx + rx * math.cos(a * 2 * math.pi / steps),
            cy + ry * math.sin(a * 2 * math.pi / steps),
        )
        for a in range(steps)
    ]


class Picture:
    def __init__(self):
        self.px = [[False] * W for _ in range(H)]
        self.ox = self.oy = 0  # everything drawn is shifted by this much

    def _mask(self, points):
        img = Image.new("1", (W, H), 0)
        ImageDraw.Draw(img).polygon(
            [(round(x + self.ox), round(y + self.oy)) for x, y in points],
            fill=1,
            outline=1,
        )
        return img.load()

    def fill(self, points, tone, clip=None):
        mask = self._mask(points)
        clip = self._mask(clip) if clip else None
        tone = TONES[tone]
        for y in range(H):
            for x in range(W):
                if mask[x, y] and (clip is None or clip[x, y]):
                    self.px[y][x] = tone(x, y)

    def stroke(self, points, closed=False, ink=True, width=1):
        img = Image.new("1", (W, H), 0)
        pts = [(round(x + self.ox), round(y + self.oy)) for x, y in points]
        if closed:
            pts.append(pts[0])
        ImageDraw.Draw(img).line(pts, fill=1, width=width)
        mask = img.load()
        for y in range(H):
            for x in range(W):
                if mask[x, y]:
                    self.px[y][x] = ink

    def shape(self, points, tone, outline=True):
        self.fill(points, tone)
        if outline:
            self.stroke(points, closed=True)

    def blob(self, points, tone, outline=True):
        self.shape(spline(points), tone, outline)

    def oval(self, cx, cy, rx, ry, tone, outline=True):
        self.shape(oval(cx, cy, rx, ry), tone, outline)

    def curve(self, points, ink=True, width=1):
        self.stroke(spline(points, closed=False), ink=ink, width=width)

    def line(self, x0, y0, x1, y1, ink=True, width=1):
        self.stroke([(x0, y0), (x1, y1)], ink=ink, width=width)

    def dot(self, x, y, ink=True):
        x, y = x + self.ox, y + self.oy
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] = ink

    def rows(self, x, y, rows, ink=True):
        """Stamp a little hand-drawn bitmap: '#' is drawn, anything else left alone."""
        for dy, row in enumerate(rows):
            for dx, ch in enumerate(row):
                if ch == "#":
                    self.dot(x + dx, y + dy, ink)
                elif ch == "o":
                    self.dot(x + dx, y + dy, not ink)

    def sparkle(self, x, y, r=2):
        for d in range(-r, r + 1):
            self.dot(x + d, y)
            self.dot(x, y + d)
        self.dot(x, y, False)


def mirror(points, cx):
    return [(2 * cx - x, y) for x, y in points]


# ---------------------------------------------------------------- Nugget


def nugget():
    """An elderly tiger-striped gentleman with white socks, sitting for his portrait."""
    p = Picture()
    cx = 46

    # The cushion he sits on
    p.oval(cx + 4, 60, 46, 5, "mid")

    # Tail, curled round to the front, ringed like the rest of him
    tail = [(66, 55), (78, 50), (91, 51), (98, 57), (92, 62), (76, 62), (66, 61)]
    p.blob(tail, "paper")
    for x in (73, 79, 85, 91):
        p.fill(
            [(x, 49), (x + 3, 49), (x + 2, 63), (x - 1, 63)], "ink", clip=spline(tail)
        )
    p.stroke(spline(tail), closed=True)

    # Body
    body = [
        (27, 62),
        (21, 52),
        (24, 40),
        (33, 31),
        (46, 28),
        (59, 31),
        (68, 40),
        (71, 52),
        (65, 62),
    ]
    p.blob(body, "paper")
    # Tiger stripes down each flank, shaded on the far side
    for y in (38, 44, 50, 56):
        stripe = [
            (20, y + 1),
            (27, y - 2),
            (34, y),
            (34, y + 2),
            (27, y + 1),
            (20, y + 4),
        ]
        p.fill(stripe, "ink", clip=spline(body))
        p.fill(mirror(stripe, cx), "ink", clip=spline(body))
    p.stroke(spline(body), closed=True)

    # Front legs, with white socks
    for x0 in (36, 48):
        p.shape([(x0 + 1, 44), (x0 + 8, 44), (x0 + 8, 62), (x0, 62)], "paper")
        for y in (47, 51):
            p.line(x0 + 1, y, x0 + 8, y, width=2)
        p.line(x0 + 1, 44, x0 + 8, 44, ink=False)
        for toe in (3, 6):
            p.line(x0 + toe, 60, x0 + toe, 62)
    # White bib between them
    p.blob(
        [(39, 34), (46, 32), (53, 34), (56, 40), (52, 45), (40, 45), (36, 40)],
        "paper",
        outline=False,
    )
    for x in (39, 43, 47, 51):  # ruffled lower edge
        p.line(x, 44, x + 1, 46)

    # Ears
    for ear in ([(30, 16), (27, 0), (42, 8)], [(50, 8), (65, 0), (62, 16)]):
        p.shape(ear, "paper")
    p.shape([(32, 12), (31, 5), (38, 9)], "dark", outline=False)
    p.shape([(54, 9), (61, 5), (60, 12)], "dark", outline=False)

    # Head, broad in the cheeks
    head = [
        (46, 5),
        (57, 7),
        (63, 14),
        (65, 22),
        (60, 30),
        (46, 34),
        (32, 30),
        (27, 22),
        (29, 14),
        (35, 7),
    ]
    p.blob(head, "paper")

    # The tabby M on his brow, and cheek stripes
    for x, top in ((41, 7), (46, 6), (51, 7)):
        p.line(x, top, x, top + 5, width=2)
    for y in (20, 24):
        left = [(27, y), (33, y + 1), (33, y + 2), (27, y + 2)]
        p.fill(left, "ink", clip=spline(head))
        p.fill(mirror(left, cx), "ink", clip=spline(head))
    p.stroke(spline(head), closed=True)

    # Eyes: half closed, content
    for ex in (39, 53):
        eye = [
            (ex - 5, 18),
            (ex - 2, 15),
            (ex + 2, 15),
            (ex + 5, 18),
            (ex + 2, 21),
            (ex - 2, 21),
        ]
        p.shape(spline(eye, steps=4), "paper")
        p.fill(
            [(ex - 5, 14), (ex + 5, 14), (ex + 5, 17), (ex - 5, 17)],
            "ink",
            clip=spline(eye, steps=4),
        )  # heavy lid
        p.fill(
            [(ex - 1, 17), (ex + 1, 17), (ex + 1, 21), (ex - 1, 21)],
            "ink",
            clip=spline(eye, steps=4),
        )

    # Nose and mouth
    p.rows(44, 24, ["#####", " ### ", "  #  "])
    p.rows(42, 27, ["    #    ", "   # #   ", "###   ###"])

    # Whiskers
    for dy, tip in ((0, -4), (1, 0), (2, 4)):
        p.line(38, 27 + dy, 18, 27 + dy + tip)
        p.line(54, 27 + dy, 74, 27 + dy + tip)
    for dx in (0, 3):  # whisker spots
        p.dot(39 + dx, 27)
        p.dot(53 - dx, 27)

    # Bow tie
    p.shape([(38, 33), (38, 41), (45, 37)], "ink")
    p.shape([(54, 33), (54, 41), (47, 37)], "ink")
    p.shape([(45, 35), (47, 35), (47, 39), (45, 39)], "ink")
    p.dot(46, 37, False)
    p.dot(40, 37, False)
    p.dot(52, 37, False)
    return p


# ---------------------------------------------------------------- Baby


def baby():
    """A chonky grey cat, loafed, with a little face and big bright eyes."""
    p = Picture()

    # Patch of sun on the floor
    p.shape([(0, 63), (12, 54), (104, 54), (92, 63)], "light", outline=False)

    # Tail
    p.blob([(82, 55), (92, 49), (100, 52), (99, 59), (90, 61), (82, 61)], "mid")

    # The loaf: wider than it is tall
    body = [
        (6, 60),
        (5, 46),
        (13, 34),
        (30, 27),
        (62, 27),
        (79, 34),
        (87, 46),
        (86, 60),
        (72, 63),
        (20, 63),
    ]
    p.blob(body, "mid")
    # Rolls of chub
    p.curve([(13, 44), (17, 52), (14, 60)], width=2)
    p.curve([(79, 44), (75, 52), (78, 60)], width=2)

    # Tucked paws
    for x in (35, 57):
        p.oval(x, 60, 7, 3, "paper")
        p.line(x - 2, 60, x - 2, 62)
        p.line(x + 2, 60, x + 2, 62)

    # Ears
    for ear in ([(32, 17), (30, 3), (43, 10)], [(49, 10), (62, 3), (60, 17)]):
        p.shape(ear, "mid")
    p.shape([(34, 13), (33, 7), (39, 11)], "paper", outline=False)
    p.shape([(53, 11), (59, 7), (58, 13)], "paper", outline=False)

    # A round head with full cheeks, and all the features bunched in the middle
    head = [
        (46, 8),
        (56, 10),
        (63, 17),
        (65, 25),
        (59, 33),
        (46, 36),
        (33, 33),
        (27, 25),
        (29, 17),
        (36, 10),
    ]
    p.blob(head, "mid")

    # Big bright eyes, set wide
    for ex in (38, 54):
        p.oval(ex, 21, 5.6, 5.6, "paper")
        p.oval(ex, 21, 5.6, 5.6, "paper")
        p.oval(ex + (1 if ex < 46 else -1), 21.5, 3.4, 3.8, "ink", outline=False)
        p.rows(ex - 2 + (1 if ex < 46 else -1), 19, ["oo", "oo"])  # catchlight
        p.dot(ex + (2 if ex < 46 else 0), 24, False)

    # A pale muzzle with a tiny nose and mouth
    p.blob(
        [(46, 25), (51, 27), (51, 31), (46, 33), (41, 31), (41, 27)],
        "paper",
        outline=False,
    )
    p.rows(44, 27, ["#####", " ### ", "  #  ", " # # ", "#   #"])

    # Whiskers
    for dy, tip in ((0, -2), (2, 2)):
        p.line(39, 30 + dy, 21, 30 + dy + tip)
        p.line(53, 30 + dy, 71, 30 + dy + tip)

    # Bright-eyed sparkles
    p.sparkle(74, 12, 3)
    p.sparkle(93, 34, 1)
    p.sparkle(17, 13, 2)
    p.sparkle(9, 23, 1)
    return p


# ---------------------------------------------------------------- Yulia


def yulia(colour):
    """Yulia and her tall fellow: dark bushy curls, a big nose, broad shoulders
    and an arm long enough to reach right round her."""
    p = Picture()
    cx = 42
    tone = {"black": "ink", "brown": "dark", "blonde": "paper"}[colour]

    # Him first, standing behind: broad shoulders that run off the page
    p.blob(
        [
            (60, 68),
            (63, 52),
            (74, 44),
            (90, 41),
            (104, 41),
            (121, 45),
            (131, 54),
            (134, 68),
        ],
        "paper",
    )
    p.shape([(91, 33), (103, 33), (103, 43), (91, 43)], "paper", outline=False)
    p.line(91, 36, 91, 42)
    p.line(103, 36, 103, 42)
    p.curve([(88, 42), (97, 49), (106, 42)])  # collar
    p.line(97, 49, 97, 64)
    for y in (53, 58):
        p.dot(99, y)
    # His long arm, round behind her and out the other side
    p.shape(
        spline([(74, 45), (52, 52), (30, 54), (2, 55)], closed=False)
        + spline([(2, 68), (30, 68), (52, 66), (74, 62)], closed=False),
        "paper",
    )

    # Her, in front of him
    p.ox, p.oy = -8, 10

    # Hair behind the face
    hair = [
        (42, 1),
        (56, 4),
        (65, 13),
        (67, 27),
        (67, 41),
        (61, 48),
        (23, 48),
        (17, 41),
        (17, 27),
        (19, 13),
        (28, 4),
    ]
    p.blob(hair, tone)

    # Neck and sweater
    p.shape([(36, 42), (48, 42), (48, 52), (36, 52)], "paper", outline=False)
    p.line(36, 44, 36, 51)
    p.line(48, 44, 48, 51)
    sweater = [
        (6, 66),
        (9, 57),
        (22, 51),
        (36, 50),
        (48, 50),
        (62, 51),
        (75, 57),
        (78, 66),
    ]
    p.blob(sweater, "paper")
    for x in range(34, 51):  # ribbed collar
        p.dot(x, 51)
        p.dot(x, 53)
        if x % 2 == 0:
            p.dot(x, 52)
    p.rows(39, 57, [" ## ## ", "#######", "#######", " ##### ", "  ###  ", "   #   "])
    for x in (14, 70):  # sleeve creases
        p.curve([(x, 58), (x + (2 if x < cx else -2), 61), (x, 64)])

    # Face: full cheeks, little chin
    face = [
        (42, 9),
        (52, 11),
        (57, 19),
        (57, 30),
        (52, 39),
        (42, 45),
        (32, 39),
        (27, 30),
        (27, 19),
        (32, 11),
    ]
    p.blob(face, "paper")

    # Fringe, swept from a rounded hairline
    fringe = [
        (27, 24),
        (27, 15),
        (33, 9),
        (42, 7),
        (52, 9),
        (57, 15),
        (57, 24),
        (53, 17),
        (45, 13),
        (37, 14),
        (31, 18),
    ]
    p.blob(fringe, tone)
    if colour == "black":
        p.curve([(28, 12), (34, 6), (43, 4)], ink=False)
        p.curve([(50, 5), (55, 8)], ink=False)
    elif colour == "blonde":
        for x in (20, 23, 61, 64):
            p.curve([(x, 20), (x - 1 if x < cx else x + 1, 32), (x, 44)])
        p.curve([(33, 8), (38, 5), (46, 5)])
    else:
        p.curve([(28, 12), (34, 6), (43, 4)], ink=False)

    # Heart clip
    p.rows(57, 9, [" # # ", "#####", "#####", " ### ", "  #  "], ink=colour != "black")

    # Round glasses
    for ex in (35, 49):
        p.oval(ex, 28, 6.4, 6.4, "paper")
        p.rows(ex - 1, 26, [" ## ", "####", "####", " ## "])
        p.dot(ex, 27, False)
    p.line(41, 27, 43, 27)
    p.line(28, 26, 26, 25)
    p.line(56, 26, 58, 25)

    # Blush and smile
    for bx in (31, 51):
        p.rows(bx, 36, ["# #", " # "])
    p.curve([(37, 38), (39, 40), (42, 41), (45, 40), (47, 38)])

    p.ox = p.oy = 0

    # His hand, come to rest on her far shoulder
    p.blob([(3, 58), (10, 55), (16, 58), (15, 63), (5, 64)], "paper")
    for x in (7, 10, 13):
        p.line(x, 59, x - 1, 63)

    # His head, well above hers: a bush of dark curls
    curls = []
    for a in range(64):
        t = a * 2 * math.pi / 64
        r = 1 + 0.09 * math.sin(t * 9) + 0.05 * math.sin(t * 15 + 1)
        curls.append((97 + 21 * r * math.cos(t), 14 + 17 * r * math.sin(t)))
    p.shape(curls, "ink")
    p.blob(
        [
            (97, 9),
            (105, 11),
            (109, 19),
            (108, 28),
            (103, 36),
            (97, 39),
            (91, 36),
            (86, 28),
            (85, 19),
            (89, 11),
        ],
        "paper",
    )
    for x, y in (
        (88, 11),
        (92, 9),
        (97, 8),
        (102, 9),
        (106, 11),
    ):  # curls over his brow
        p.oval(x, y, 3.4, 3.2, "ink")
    for x, y in (
        (80, 8),
        (86, 3),
        (95, 1),
        (104, 3),
        (112, 7),
        (115, 15),
        (79, 16),
        (90, 6),
        (108, 2),
        (100, 5),
    ):
        p.rows(x, y, [" oo", "o  ", " oo"])  # the shine on each curl
    p.oval(84, 23, 2, 3, "paper")
    p.oval(110, 23, 2, 3, "paper")
    # Heavy brows, kind eyes
    p.line(88, 17, 94, 16, width=2)
    p.line(100, 16, 106, 17, width=2)
    p.rows(90, 20, ["##", "##"])
    p.rows(102, 20, ["##", "##"])
    # The nose
    p.curve(
        [
            (95, 19),
            (94, 25),
            (92, 29),
            (93, 32),
            (97, 33),
            (101, 32),
            (103, 29),
            (101, 25),
            (100, 19),
        ]
    )
    p.curve([(93, 30), (95, 31)])
    p.curve([(102, 30), (100, 31)])
    # And a lopsided smile
    p.curve([(91, 35), (95, 37), (100, 37), (104, 34)])
    return p


# ---------------------------------------------------------------- window


def window(night):
    """The view from the window, with Nugget on the sill keeping watch."""
    p = Picture()
    sky, land = ("ink", False) if night else ("paper", True)

    if night:
        p.shape([(0, 0), (127, 0), (127, 63), (0, 63)], "ink", outline=False)
        for x, y in (
            (12, 8),
            (25, 17),
            (36, 6),
            (50, 12),
            (74, 7),
            (84, 9),
            (112, 7),
            (118, 15),
            (30, 9),
            (110, 24),
            (72, 22),
            (20, 8),
        ):
            p.dot(x, y, False)
        # Crescent moon
        p.oval(46, 14, 7, 7, "paper", outline=False)
        p.oval(49, 12, 6, 6, "ink", outline=False)
    else:
        # Sun
        p.oval(46, 13, 5, 5, "paper")
        for a in range(8):
            ca, sa = math.cos(a * math.pi / 4), math.sin(a * math.pi / 4)
            p.line(46 + ca * 8, 13 + sa * 8, 46 + ca * 10, 13 + sa * 10)
        # A cloud
        cloud = [
            (72, 12),
            (74, 9),
            (78, 8),
            (81, 6),
            (86, 6),
            (89, 9),
            (93, 10),
            (94, 12),
        ]
        p.curve(cloud)
        p.line(72, 12, 94, 12)
        # Birds on the wing
        for bx, by in ((104, 8), (112, 13)):
            p.rows(bx, by, ["#   #", " # # ", "  #  "])

    # The mountain, with snow on top
    peak = [(68, 46), (94, 15), (99, 21), (103, 18), (126, 46)]
    p.shape(peak, "light" if not night else "mid", outline=False)
    p.stroke(peak, ink=land)
    snow = [
        (94, 15),
        (87, 24),
        (91, 22),
        (94, 26),
        (98, 23),
        (102, 27),
        (107, 23),
        (103, 18),
        (99, 21),
    ]
    p.shape(snow, "paper", outline=False)
    p.stroke([(87, 24), (91, 22), (94, 26), (98, 23), (102, 27), (107, 23)], ink=True)
    p.stroke([(87, 24), (94, 15), (99, 21), (103, 18), (107, 23)], ink=True)

    # Rolling hills
    hills = [
        (0, 46),
        (14, 41),
        (30, 44),
        (48, 40),
        (70, 45),
        (92, 40),
        (112, 44),
        (127, 41),
        (127, 63),
        (0, 63),
    ]
    p.fill(spline(hills[:8], closed=False) + [(127, 63), (0, 63)], "dark")
    p.stroke(spline(hills[:8], closed=False), ink=land)

    # Fir trees and rooftops
    for tx, th in ((8, 14), (15, 10), (116, 13), (122, 9), (80, 9)):
        for k in range(3):
            w = 2 + k * 2
            y = 52 - th + k * (th // 3)
            p.shape(
                [(tx, y), (tx - w, y + th // 3 + 1), (tx + w, y + th // 3 + 1)],
                "ink",
                outline=False,
            )
        p.line(tx, 52, tx, 54)
    for hx, hw, hh in ((24, 14, 9), (44, 18, 12), (66, 12, 8), (90, 20, 11)):
        top = 56 - hh
        p.shape(
            [(hx, top), (hx + hw, top), (hx + hw, 56), (hx, 56)], "ink", outline=False
        )
        p.shape(
            [(hx - 2, top), (hx + hw // 2, top - 6), (hx + hw + 2, top)],
            "ink",
            outline=False,
        )
        p.shape(
            [
                (hx + hw - 5, top - 6),
                (hx + hw - 3, top - 6),
                (hx + hw - 3, top - 1),
                (hx + hw - 5, top - 1),
            ],
            "ink",
            outline=False,
        )
        if night:
            p.stroke(
                [(hx - 2, top), (hx + hw // 2, top - 6), (hx + hw + 2, top)], ink=False
            )
        for wx in range(hx + 3, hx + hw - 3, 5):  # windows
            p.rows(wx, top + 3, ["##", "##"], ink=False)
            if not night:
                p.dot(wx + 1, top + 4, True)

    # A branch across the corner, with two birds on it (asleep at night)
    p.curve([(0, 20), (14, 23), (30, 22), (44, 26)], ink=land, width=2)
    p.curve([(18, 23), (24, 17), (30, 15)], ink=land)
    p.curve([(32, 23), (38, 19)], ink=land)
    for lx, ly in ((30, 14), (38, 18), (44, 27), (25, 16)):
        p.rows(lx, ly - 1, [" ## ", "####", " ## "], ink=land)
    for bx in (10, 21):
        by = 15 if bx == 10 else 16
        bird = ["  ###  ", " ##### ", "#######", "###### ", " ##### ", "  # #  "]
        p.rows(bx, by, bird, ink=land)
        if not night:
            p.dot(bx + 4, by + 1, False)
            p.rows(bx + 7, by + 2, ["#"], ink=True)

    # Nugget on the sill, seen from behind
    cat = [
        (22, 63),
        (20, 54),
        (22, 46),
        (28, 42),
        (36, 42),
        (42, 46),
        (44, 54),
        (42, 63),
    ]
    p.blob(cat, "ink", outline=False)
    p.shape([(22, 47), (22, 37), (30, 43)], "ink", outline=False)
    p.shape([(34, 43), (42, 37), (42, 47)], "ink", outline=False)
    p.blob(
        [(40, 60), (52, 56), (62, 58), (64, 62), (52, 63), (40, 63)],
        "ink",
        outline=False,
    )
    # A pale edge so he stands out against the dark
    p.stroke(spline(cat, closed=True)[8:62], ink=False)
    p.stroke([(22, 47), (22, 37), (30, 43)], ink=False)
    p.stroke([(34, 43), (42, 37), (42, 47)], ink=False)
    p.curve([(42, 59), (52, 55), (62, 57), (65, 62)], ink=False)
    for y in (48, 52, 56):  # stripes down his back
        p.line(28, y, 36, y, ink=False)

    # The window itself: frame, glazing bars and sill
    for inset in range(3):
        p.stroke(
            [
                (inset, inset),
                (W - 1 - inset, inset),
                (W - 1 - inset, H - 1 - inset),
                (inset, H - 1 - inset),
            ],
            closed=True,
        )
    p.stroke([(3, 3), (W - 4, 3), (W - 4, H - 4), (3, H - 4)], closed=True, ink=False)
    for x in (62, 63, 64, 65):
        p.line(x, 3, x, 60, ink=x in (63, 64))
    for y in (27, 28, 29, 30):
        p.line(3, y, 124, y, ink=y in (28, 29))
    return p


# ---------------------------------------------------------------- abstracts


def rect(cx, cy, w, h, angle):
    """A rectangle about its centre, turned by `angle` degrees."""
    ca, sa = math.cos(math.radians(angle)), math.sin(math.radians(angle))
    return [
        (cx + x * ca - y * sa, cy + x * sa + y * ca)
        for x, y in ((-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2))
    ]


def black_cat():
    """After Malevich: a black square, which on a second look is a black cat at night."""
    p = Picture()
    p.shape([(40, 10), (87, 10), (87, 57), (40, 57)], "ink")
    # Two ears break the top edge, just
    p.shape([(46, 10), (49, 5), (55, 10)], "ink")
    p.shape([(72, 10), (78, 5), (81, 10)], "ink")
    # And two eyes look back
    for ex in (54, 73):
        eye = spline([(ex - 5, 27), (ex, 24), (ex + 5, 27), (ex, 30)], steps=5)
        p.fill(eye, "paper")
        p.line(ex, 25, ex, 29, width=2)
    # A canvas edge and a signature
    p.stroke([(36, 3), (91, 3), (91, 61), (36, 61)], closed=True)
    p.rows(82, 58, ["# # #", " # # "], ink=True)
    return p


def composition_yarn():
    """After Kandinsky: circles, arcs and straight lines, and one ball of yarn."""
    p = Picture()
    p.oval(34, 26, 21, 21, "paper")
    p.oval(34, 26, 21, 21, "light")
    p.oval(27, 21, 9, 9, "ink")
    p.oval(44, 36, 5, 5, "paper")
    p.oval(92, 16, 11, 11, "dark")
    p.oval(97, 12, 4, 4, "paper")
    p.oval(70, 48, 7, 7, "mid")
    # Straight lines through everything
    p.line(2, 58, 120, 4, width=2)
    p.line(10, 6, 84, 62)
    p.line(60, 2, 60, 40)
    p.line(64, 2, 64, 34)
    p.line(68, 2, 68, 28)
    # A chequered wedge and a triangle
    for i in range(5):
        for j in range(3):
            if (i + j) % 2 == 0:
                p.shape(
                    rect(100 + i * 4 - j * 2, 40 + j * 4 + i, 4, 4, 20),
                    "ink",
                    outline=False,
                )
    p.shape([(78, 30), (90, 34), (80, 42)], "paper")
    p.shape([(8, 38), (20, 44), (6, 52)], "dark")
    # Arcs
    p.curve([(46, 58), (58, 50), (72, 58), (86, 50), (100, 58)], width=2)
    p.curve([(104, 26), (114, 30), (120, 40), (118, 52)])
    p.curve([(108, 24), (119, 29), (125, 40), (123, 54)])
    # The yarn, with its loose end wandering off
    p.oval(112, 54, 7, 7, "paper")
    for d in (-4, -1, 2):
        p.curve([(106, 54 + d), (112, 51 + d), (118, 54 + d)])
    p.curve([(109, 49), (112, 54), (110, 60)])
    p.curve([(106, 58), (96, 62), (88, 56), (80, 60), (74, 57)])
    return p


def suprematist_dinner():
    """After Malevich again: tilted blocks that turn out to be dinner."""
    p = Picture()
    p.shape(rect(52, 34, 70, 30, -14), "ink")  # the mat
    p.shape(rect(96, 18, 34, 7, 28), "dark")
    p.shape(rect(20, 14, 22, 5, -34), "mid")
    p.shape(rect(108, 46, 10, 10, 12), "ink")
    p.shape(rect(120, 30, 5, 14, 12), "ink")
    p.shape(rect(12, 50, 16, 4, 40), "ink")
    p.line(4, 30, 30, 4)
    p.line(70, 60, 126, 52)
    # The bowl, seen from above
    p.oval(40, 36, 13, 13, "paper")
    p.oval(40, 36, 9, 9, "paper")
    for a in range(0, 360, 40):  # kibble
        r = 4 if a % 80 else 1
        p.dot(
            round(40 + r * math.cos(math.radians(a))),
            round(36 + r * math.sin(math.radians(a))),
        )
    # The fish
    p.shape(spline([(60, 32), (70, 24), (82, 26), (72, 36)], steps=6), "paper")
    p.shape([(81, 26), (90, 18), (90, 30)], "paper")
    p.dot(64, 30)
    p.line(70, 27, 72, 33)
    p.line(74, 26, 76, 32)
    return p


def cubist_baby():
    """Baby, taken apart and put back together at odd angles."""
    p = Picture()
    # Planes of grey that add up to a cat's head, more or less
    p.shape([(28, 60), (22, 30), (46, 12), (64, 34), (58, 62)], "mid")
    p.shape([(46, 12), (84, 8), (100, 30), (64, 34)], "light")
    p.shape([(64, 34), (100, 30), (106, 56), (58, 62)], "dark")
    p.shape([(22, 30), (10, 44), (28, 60)], "dark")
    p.shape([(84, 8), (112, 14), (100, 30)], "paper")
    # Ears, not where they should be
    p.shape([(30, 24), (20, 4), (44, 13)], "paper")
    p.shape([(88, 9), (104, 0), (108, 13)], "ink")
    p.shape([(8, 46), (2, 36), (14, 40)], "ink")
    # One eye full on, one in profile
    p.oval(44, 34, 9, 9, "paper")
    p.oval(45, 35, 5.5, 6.5, "ink", outline=False)
    p.rows(42, 31, ["oo", "oo"])
    eye = spline([(72, 20), (82, 15), (92, 20), (82, 24)], steps=6)
    p.shape(eye, "paper")
    p.oval(84, 20, 2.5, 4, "ink", outline=False)
    # Nose on the cheek, mouth further down
    p.shape([(72, 40), (82, 40), (77, 46)], "ink")
    p.shape([(62, 48), (94, 46), (96, 56), (64, 58)], "paper")
    p.curve([(66, 50), (72, 55), (78, 50), (84, 55), (90, 50)])
    # Whiskers from one corner
    for ty in (38, 46, 54, 62):
        p.line(104, 44, 127, ty)
    for ty in (48, 55, 62):
        p.line(20, 50, 0, ty)
    # A paw, and the planes drawn back in
    p.oval(116, 22, 6, 5, "mid")
    for dx in (-3, 0, 3):
        p.line(116 + dx, 22, 116 + dx, 26)
    p.line(46, 12, 64, 34, width=2)
    p.line(64, 34, 58, 62, width=2)
    p.line(64, 34, 100, 30, width=2)
    return p


def broadway_zoomies():
    """After Mondrian: a grid of streets, and the route Baby took round it."""
    p = Picture()
    xs = (14, 40, 58, 92, 112)
    ys = (10, 26, 46)
    # Blocks of tone in some of the cells
    p.shape([(15, 0), (39, 0), (39, 9), (15, 9)], "ink", outline=False)
    p.shape([(59, 11), (91, 11), (91, 25), (59, 25)], "dark", outline=False)
    p.shape([(0, 27), (13, 27), (13, 45), (0, 45)], "mid", outline=False)
    p.shape([(93, 47), (111, 47), (111, 63), (93, 63)], "ink", outline=False)
    p.shape([(41, 47), (57, 47), (57, 63), (41, 63)], "light", outline=False)
    p.shape([(113, 0), (127, 0), (127, 9), (113, 9)], "mid", outline=False)
    # Streets: double lines beaded with little blocks
    for x in xs:
        p.line(x - 1, 0, x - 1, 63)
        p.line(x + 1, 0, x + 1, 63)
        for y in range(1, 63, 6):
            p.shape(
                [(x - 1, y), (x + 1, y), (x + 1, y + 2), (x - 1, y + 2)],
                "ink",
                outline=False,
            )
    for y in ys:
        p.line(0, y - 1, 127, y - 1)
        p.line(0, y + 1, 127, y + 1)
        for x in range(2, 127, 7):
            p.shape(
                [(x, y - 1), (x + 2, y - 1), (x + 2, y + 1), (x, y + 1)],
                "ink",
                outline=False,
            )
    # A paw print in one block
    p.oval(75, 38, 3.5, 3, "ink")
    for dx, dy in ((-5, -3), (-2, -6), (2, -6), (5, -3)):
        p.oval(75 + dx, 38 + dy, 1.2, 1.2, "ink")
    # The zoomies: a dashed dash round the blocks
    route = spline(
        [
            (4, 58),
            (22, 36),
            (30, 18),
            (48, 6),
            (50, 36),
            (70, 56),
            (84, 34),
            (102, 18),
            (122, 36),
            (104, 40),
        ],
        closed=False,
        steps=12,
    )
    for i in range(0, len(route) - 2, 4):
        p.stroke(route[i : i + 3], ink=True, width=2)
    return p


# ---------------------------------------------------------------- giraffes


def giraffe(p, ox, ground, k, flip):
    """One giraffe, feet on the ground at ox, facing right unless flipped."""

    def t(pt):
        x, y = pt
        return (ox + (x - 45) * k * (-1 if flip else 1), ground + (y - 62) * k)

    def tt(points):
        return [t(q) for q in points]

    # Tail
    p.curve(tt([(31, 38), (27, 44), (27, 51)]))
    p.shape(tt([(26, 51), (28, 51), (27, 56)]), "ink")
    # Legs, far pair then near pair
    for x0, bend in ((37, 1), (55, -1), (33, 0), (51, 0)):
        leg = [
            (x0, 45),
            (x0 + 3, 45),
            (x0 + 3 + bend, 54),
            (x0 + 3, 62),
            (x0, 62),
            (x0 + bend, 54),
        ]
        p.shape(tt(leg), "paper")
        p.shape(tt([(x0, 60), (x0 + 3, 60), (x0 + 3, 62), (x0, 62)]), "ink")
    # Body, neck and head in one outline
    body = [
        (30, 37),
        (38, 33),
        (50, 33),
        (57, 36),
        (60, 42),
        (57, 48),
        (42, 49),
        (31, 46),
    ]
    neck = [(51, 35), (59, 39), (71, 12), (66, 9)]
    head = [(66, 9), (67, 5), (71, 3), (77, 5), (82, 8), (81, 11), (74, 12), (69, 12)]
    p.shape(tt(neck), "paper")
    p.blob(tt(body), "paper")
    p.shape(
        tt([(52, 35), (58, 38), (60, 34)]), "paper", outline=False
    )  # blend the shoulder
    p.blob(tt(head), "paper")
    # Patches
    spots = [
        (35, 38),
        (41, 36),
        (47, 37),
        (53, 39),
        (37, 43),
        (44, 42),
        (50, 44),
        (56, 44),
        (40, 47),
        (55, 37),
        (58, 33),
        (60, 28),
        (62, 23),
        (65, 18),
        (67, 13),
        (61, 34),
        (64, 26),
        (66, 21),
    ]
    for i, (sx, sy) in enumerate(spots):
        r = 2.2 if sy > 34 else 1.5
        patch = [
            (sx - r, sy - r * 0.6),
            (sx, sy - r),
            (sx + r, sy - r * 0.4),
            (sx + r * 0.7, sy + r),
            (sx - r * 0.6, sy + r * 0.8),
        ]
        p.fill(tt(patch), "ink", clip=spline(tt(body)) if sy > 34 else tt(neck))
    p.stroke(spline(tt(body)), closed=True)
    p.stroke(tt([(51, 35), (66, 9)]))
    p.stroke(tt([(59, 39), (71, 12)]))
    # Mane, horns, ear, eye
    for i in range(6):
        mx, my = 52 + i * 2.6, 33 - i * 4.2
        p.stroke(tt([(mx, my), (mx - 1.5, my - 1.5)]))
    for hx in (69, 72):
        p.stroke(tt([(hx, 4), (hx - 0.5, 0)]))
        p.shape(
            tt([(hx - 1.5, -1), (hx + 0.5, -1), (hx + 0.5, 1), (hx - 1.5, 1)]), "ink"
        )
    p.shape(tt([(66, 7), (62, 5), (65, 9)]), "paper")
    ex, ey = t((73, 7))
    p.dot(round(ex), round(ey))
    nx, ny = t((80, 9))
    p.dot(round(nx), round(ny))
    p.stroke(tt([(77, 11), (80, 11)]))


def giraffes():
    """Two giraffes on the savanna, and the tree they have been eating."""
    p = Picture()
    # Sun, low and large
    p.oval(84, 14, 9, 9, "light")
    # Flat-topped acacia, off to one side
    p.shape(
        [
            (119, 62),
            (120, 36),
            (116, 27),
            (119, 27),
            (122, 34),
            (125, 26),
            (127, 27),
            (124, 36),
            (125, 62),
        ],
        "ink",
        outline=False,
    )
    canopy = [
        (102, 25),
        (106, 19),
        (116, 16),
        (127, 16),
        (134, 19),
        (136, 25),
        (126, 27),
        (112, 27),
    ]
    p.blob(canopy, "dark")
    # Far hills and the ground
    p.curve([(0, 50), (20, 46), (44, 49), (70, 45), (96, 48), (127, 44)])
    p.line(0, 62, 127, 62)
    for gx in (4, 15, 27, 62, 70, 84, 99, 122):
        p.line(gx, 62, gx - 1, 59)
        p.line(gx, 62, gx + 1, 58)
        p.line(gx, 62, gx + 2, 60)
    giraffe(p, 28, 62, 1.0, False)
    giraffe(p, 95, 62, 0.8, True)
    # Birds
    for bx, by in ((52, 8), (60, 12), (8, 40)):
        p.rows(bx, by, ["#   #", " # # ", "  #  "])
    return p


# ---------------------------------------------------------------- output


def pictures():
    return {
        "nugget": nugget(),
        "baby": baby(),
        "yulia_black": yulia("black"),
        "yulia_brown": yulia("brown"),
        "yulia_blonde": yulia("blonde"),
        "window_day": window(False),
        "window_night": window(True),
        "black_cat": black_cat(),
        "composition": composition_yarn(),
        "dinner": suprematist_dinner(),
        "cubist": cubist_baby(),
        "broadway": broadway_zoomies(),
        "giraffes": giraffes(),
    }


def xbm(picture):
    out = []
    for y in range(H):
        for bx in range(W // 8):
            byte = 0
            for bit in range(8):
                if picture.px[y][bx * 8 + bit]:
                    byte |= 1 << bit
            out.append(byte)
    return out


def write_header(pics, path):
    out = [
        "// Generated by tools/build_art.py - do not edit by hand.\n",
        "#pragma once\n\n#include <stdint.h>\n\n",
        f"#define ART_W {W}\n#define ART_H {H}\n\n",
    ]
    for name, pic in pics.items():
        data = xbm(pic)
        out.append(f"static const uint8_t art_{name}[] = {{\n")
        for i in range(0, len(data), 16):
            out.append(
                "    " + ", ".join(f"0x{b:02X}" for b in data[i : i + 16]) + ",\n"
            )
        out.append("};\n\n")
    path.write_text("".join(out))


def write_preview(pics, path, scale=4):
    cols = 2
    rows = -(-len(pics) // cols)
    sheet = Image.new("RGB", (cols * (W + 2) - 2, rows * (H + 2) - 2), (255, 255, 255))
    for i, pic in enumerate(pics.values()):
        ox, oy = (i % cols) * (W + 2), (i // cols) * (H + 2)
        for y in range(H):
            for x in range(W):
                sheet.putpixel(
                    (ox + x, oy + y), (0, 0, 0) if pic.px[y][x] else (255, 130, 0)
                )
    sheet.resize((sheet.width * scale, sheet.height * scale), Image.NEAREST).save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    args = ap.parse_args()
    pics = pictures()
    write_header(pics, ROOT / "art.h")
    if args.preview:
        write_preview(pics, args.preview)
    print(f"{len(pics)} pictures -> art.h")


if __name__ == "__main__":
    main()

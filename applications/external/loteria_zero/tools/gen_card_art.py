#!/usr/bin/env python3
"""Generate original 1-bit Flipper-friendly card illustrations."""

from __future__ import annotations

import json
import math
from pathlib import Path

from PIL import Image, ImageDraw


SIZE = 48
BLACK = 0
WHITE = 1


def new_canvas() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    img = Image.new("1", (SIZE, SIZE), WHITE)
    draw = ImageDraw.Draw(img)
    # Decorative border inspired by classic lotería cards
    draw.rectangle((0, 0, SIZE - 1, SIZE - 1), outline=BLACK)
    draw.rectangle((2, 2, SIZE - 3, SIZE - 3), outline=BLACK)
    return img, draw


def save(img: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)


def draw_gallo(d: ImageDraw.ImageDraw) -> None:
    # Classic side-facing rooster: comb, beak, wattle, tail fan, spur.
    # Body
    d.ellipse((14, 18, 34, 36), outline=BLACK)
    d.ellipse((18, 22, 30, 32), outline=BLACK)
    # Head
    d.ellipse((28, 10, 40, 22), outline=BLACK)
    d.ellipse((35, 14, 37, 16), fill=BLACK)  # eye
    # Comb
    d.polygon([(30, 10), (32, 4), (34, 10)], outline=BLACK)
    d.polygon([(33, 10), (36, 3), (38, 11)], outline=BLACK)
    d.polygon([(36, 11), (40, 5), (41, 12)], outline=BLACK)
    # Beak
    d.polygon([(40, 15), (46, 16), (40, 18)], outline=BLACK)
    # Wattle
    d.arc((36, 18, 42, 26), 20, 200, fill=BLACK)
    # Neck
    d.line((30, 20, 26, 26), fill=BLACK)
    d.line((34, 20, 30, 26), fill=BLACK)
    # Tail feathers (fan)
    d.arc((4, 14, 20, 36), 200, 40, fill=BLACK)
    d.arc((6, 12, 22, 34), 210, 30, fill=BLACK)
    d.arc((8, 10, 24, 32), 220, 20, fill=BLACK)
    d.line((12, 16, 18, 24), fill=BLACK)
    d.line((10, 20, 18, 26), fill=BLACK)
    # Legs + spur
    d.line((22, 36, 20, 44), fill=BLACK)
    d.line((28, 36, 30, 44), fill=BLACK)
    d.line((20, 44, 16, 44), fill=BLACK)
    d.line((30, 44, 34, 44), fill=BLACK)
    d.line((28, 40, 32, 38), fill=BLACK)


def draw_diablito(d: ImageDraw.ImageDraw) -> None:
    d.polygon([(18, 14), (24, 6), (22, 16)], outline=BLACK)
    d.polygon([(30, 14), (24, 6), (26, 16)], outline=BLACK)
    d.ellipse((16, 14, 32, 30), outline=BLACK)
    d.ellipse((20, 20, 22, 22), fill=BLACK)
    d.ellipse((26, 20, 28, 22), fill=BLACK)
    d.arc((20, 22, 28, 28), 20, 160, fill=BLACK)
    d.polygon([(22, 30), (26, 30), (28, 42), (20, 42)], outline=BLACK)
    d.line((28, 34, 36, 40), fill=BLACK)
    d.line((36, 40, 34, 42), fill=BLACK)


def draw_dama(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((18, 8, 30, 20), outline=BLACK)
    d.arc((14, 6, 34, 18), 200, 340, fill=BLACK)
    d.polygon([(16, 20), (32, 20), (36, 40), (12, 40)], outline=BLACK)
    d.line((18, 28, 30, 28), fill=BLACK)
    d.ellipse((21, 12, 23, 14), fill=BLACK)
    d.ellipse((25, 12, 27, 14), fill=BLACK)


def draw_catrin(d: ImageDraw.ImageDraw) -> None:
    d.rectangle((14, 8, 34, 14), outline=BLACK)
    d.rectangle((20, 4, 28, 8), outline=BLACK)
    d.ellipse((18, 14, 30, 26), outline=BLACK)
    d.arc((20, 18, 28, 24), 10, 170, fill=BLACK)
    d.polygon([(16, 26), (32, 26), (30, 42), (18, 42)], outline=BLACK)
    d.line((16, 32, 32, 32), fill=BLACK)


def draw_paraguas(d: ImageDraw.ImageDraw) -> None:
    d.arc((8, 10, 40, 34), 200, 340, fill=BLACK)
    d.arc((8, 12, 40, 36), 200, 340, fill=BLACK)
    d.line((24, 12, 24, 40), fill=BLACK)
    d.arc((24, 38, 30, 44), 0, 180, fill=BLACK)


def draw_sirena(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((18, 8, 30, 20), outline=BLACK)
    d.polygon([(16, 20), (32, 20), (28, 30), (20, 30)], outline=BLACK)
    d.polygon([(20, 30), (28, 30), (34, 42), (24, 36), (14, 42)], outline=BLACK)
    d.arc((6, 34, 42, 46), 200, 340, fill=BLACK)


def draw_escalera(d: ImageDraw.ImageDraw) -> None:
    d.line((14, 40, 22, 8), fill=BLACK)
    d.line((34, 40, 26, 8), fill=BLACK)
    for y in (14, 22, 30, 38):
        x0 = 14 + int((40 - y) * 0.25)
        x1 = 34 - int((40 - y) * 0.25)
        d.line((x0, y, x1, y), fill=BLACK)


def draw_botella(d: ImageDraw.ImageDraw) -> None:
    d.rectangle((20, 8, 28, 14), outline=BLACK)
    d.rectangle((18, 14, 30, 18), outline=BLACK)
    d.polygon([(16, 18), (32, 18), (34, 42), (14, 42)], outline=BLACK)
    d.ellipse((20, 24, 28, 34), outline=BLACK)


def draw_barril(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((12, 10, 36, 18), outline=BLACK)
    d.rectangle((12, 14, 36, 36), outline=BLACK)
    d.ellipse((12, 32, 36, 40), outline=BLACK)
    d.line((12, 22, 36, 22), fill=BLACK)
    d.line((12, 28, 36, 28), fill=BLACK)


def draw_arbol(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((12, 8, 36, 28), outline=BLACK)
    d.ellipse((10, 16, 28, 34), outline=BLACK)
    d.ellipse((20, 16, 38, 34), outline=BLACK)
    d.rectangle((22, 32, 26, 44), outline=BLACK)


def draw_melon(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((10, 14, 38, 38), outline=BLACK)
    d.arc((14, 16, 34, 36), 40, 140, fill=BLACK)
    d.arc((14, 16, 34, 36), 220, 320, fill=BLACK)
    d.line((24, 12, 24, 16), fill=BLACK)
    d.ellipse((22, 8, 26, 12), outline=BLACK)


def draw_valiente(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((18, 8, 30, 20), outline=BLACK)
    d.polygon([(16, 20), (32, 20), (34, 36), (14, 36)], outline=BLACK)
    d.line((14, 24, 8, 30), fill=BLACK)
    d.line((34, 24, 40, 30), fill=BLACK)
    d.line((20, 36, 20, 44), fill=BLACK)
    d.line((28, 36, 28, 44), fill=BLACK)
    d.line((8, 18, 14, 22), fill=BLACK)


def draw_gorrito(d: ImageDraw.ImageDraw) -> None:
    d.arc((10, 16, 38, 40), 200, 340, fill=BLACK)
    d.ellipse((18, 12, 30, 22), outline=BLACK)
    d.line((10, 28, 38, 28), fill=BLACK)
    d.ellipse((22, 8, 26, 12), fill=BLACK)


def draw_muerte(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((16, 8, 32, 24), outline=BLACK)
    d.ellipse((20, 14, 22, 16), fill=BLACK)
    d.ellipse((26, 14, 28, 16), fill=BLACK)
    d.arc((20, 16, 28, 22), 20, 160, fill=BLACK)
    d.rectangle((20, 24, 28, 30), outline=BLACK)
    d.line((12, 18, 16, 22), fill=BLACK)
    d.line((12, 18, 10, 30), fill=BLACK)
    d.line((10, 30, 14, 30), fill=BLACK)
    d.polygon([(18, 30), (30, 30), (32, 42), (16, 42)], outline=BLACK)


def draw_pera(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((16, 18, 32, 40), outline=BLACK)
    d.ellipse((18, 12, 30, 24), outline=BLACK)
    d.line((24, 8, 24, 14), fill=BLACK)
    d.ellipse((24, 6, 30, 12), outline=BLACK)


def draw_bandera(d: ImageDraw.ImageDraw) -> None:
    d.line((12, 8, 12, 42), fill=BLACK)
    d.rectangle((12, 10, 38, 28), outline=BLACK)
    d.line((12, 16, 38, 16), fill=BLACK)
    d.line((12, 22, 38, 22), fill=BLACK)
    d.ellipse((22, 17, 28, 23), outline=BLACK)


def draw_bandolon(d: ImageDraw.ImageDraw) -> None:
    # Classic bandolon / mandolin: teardrop body, sound hole, bridge, fretted neck, pegbox.
    # Body (pear / teardrop)
    d.ellipse((8, 16, 34, 42), outline=BLACK)
    d.ellipse((12, 20, 30, 38), outline=BLACK)
    # Sound hole
    d.ellipse((17, 26, 25, 34), outline=BLACK)
    d.ellipse((19, 28, 23, 32), fill=BLACK)
    # Bridge
    d.line((14, 36, 28, 36), fill=BLACK)
    d.line((14, 37, 28, 37), fill=BLACK)
    # Strings over body
    d.line((19, 30, 16, 36), fill=BLACK)
    d.line((21, 30, 20, 36), fill=BLACK)
    d.line((23, 30, 24, 36), fill=BLACK)
    d.line((25, 30, 28, 36), fill=BLACK)
    # Neck
    d.polygon([(26, 18), (42, 8), (44, 12), (28, 22)], outline=BLACK)
    # Frets
    d.line((30, 16, 32, 20), fill=BLACK)
    d.line((34, 13, 36, 17), fill=BLACK)
    d.line((38, 11, 40, 15), fill=BLACK)
    # Strings on neck
    d.line((28, 19, 42, 9), fill=BLACK)
    d.line((29, 21, 43, 11), fill=BLACK)
    # Pegbox + pegs
    d.polygon([(41, 8), (45, 5), (46, 10), (43, 12)], outline=BLACK)
    d.ellipse((43, 4, 45, 6), outline=BLACK)
    d.ellipse((45, 7, 47, 9), outline=BLACK)
    d.ellipse((42, 10, 44, 12), outline=BLACK)


def draw_violoncello(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((14, 16, 34, 40), outline=BLACK)
    d.rectangle((22, 8, 26, 18), outline=BLACK)
    d.line((18, 24, 30, 24), fill=BLACK)
    d.line((24, 18, 24, 36), fill=BLACK)
    d.ellipse((21, 26, 27, 32), outline=BLACK)


def draw_garza(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((22, 8, 30, 16), outline=BLACK)
    d.line((26, 16, 24, 28), fill=BLACK)
    d.polygon([(18, 24), (30, 22), (34, 30), (16, 32)], outline=BLACK)
    d.line((20, 32, 18, 42), fill=BLACK)
    d.line((28, 30, 30, 42), fill=BLACK)
    d.line((30, 12, 38, 10), fill=BLACK)


def draw_pajaro(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((14, 18, 34, 34), outline=BLACK)
    d.ellipse((30, 14, 40, 24), outline=BLACK)
    d.polygon([(40, 18), (46, 18), (40, 22)], outline=BLACK)
    d.polygon([(16, 22), (8, 16), (12, 28)], outline=BLACK)
    d.ellipse((34, 16, 36, 18), fill=BLACK)
    d.line((22, 34, 20, 42), fill=BLACK)
    d.line((26, 34, 28, 42), fill=BLACK)


def draw_mano(d: ImageDraw.ImageDraw) -> None:
    d.rectangle((16, 24, 32, 40), outline=BLACK)
    for x in (16, 20, 24, 28):
        d.rectangle((x, 10, x + 3, 26), outline=BLACK)
    d.polygon([(32, 26), (40, 22), (40, 30), (32, 32)], outline=BLACK)


def draw_bota(d: ImageDraw.ImageDraw) -> None:
    d.rectangle((18, 10, 30, 28), outline=BLACK)
    d.polygon([(18, 28), (34, 28), (38, 38), (14, 38)], outline=BLACK)
    d.line((18, 18, 30, 18), fill=BLACK)


def draw_luna(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((12, 10, 36, 34), outline=BLACK)
    d.ellipse((18, 10, 42, 34), fill=WHITE, outline=WHITE)
    d.ellipse((18, 10, 42, 34), outline=BLACK)
    d.point((10, 12), fill=BLACK)
    d.point((38, 16), fill=BLACK)
    d.point((14, 36), fill=BLACK)


def draw_cotorro(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((14, 16, 34, 36), outline=BLACK)
    d.ellipse((28, 10, 40, 22), outline=BLACK)
    d.polygon([(40, 14), (46, 16), (40, 20)], outline=BLACK)
    d.ellipse((34, 12, 36, 14), fill=BLACK)
    d.arc((18, 24, 30, 32), 20, 160, fill=BLACK)
    d.line((20, 36, 18, 44), fill=BLACK)
    d.line((28, 36, 30, 44), fill=BLACK)


def draw_borracho(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((16, 8, 28, 20), outline=BLACK)
    d.polygon([(14, 20), (30, 18), (34, 34), (12, 36)], outline=BLACK)
    d.line((12, 36, 10, 44), fill=BLACK)
    d.line((30, 34, 36, 42), fill=BLACK)
    d.ellipse((32, 22, 40, 34), outline=BLACK)
    d.line((36, 22, 36, 18), fill=BLACK)


def draw_negrito(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((16, 12, 32, 28), outline=BLACK, fill=BLACK)
    d.ellipse((20, 18, 22, 20), fill=WHITE)
    d.ellipse((26, 18, 28, 20), fill=WHITE)
    d.arc((20, 20, 28, 26), 20, 160, fill=WHITE)
    d.rectangle((14, 8, 34, 14), fill=BLACK)
    d.polygon([(16, 28), (32, 28), (34, 42), (14, 42)], outline=BLACK)


def draw_corazon(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((10, 12, 26, 28), outline=BLACK)
    d.ellipse((22, 12, 38, 28), outline=BLACK)
    d.polygon([(10, 20), (38, 20), (24, 42)], outline=BLACK)


def draw_sandia(d: ImageDraw.ImageDraw) -> None:
    d.pieslice((8, 12, 40, 44), 200, 340, outline=BLACK)
    d.arc((12, 16, 36, 40), 200, 340, fill=BLACK)
    for x, y in ((18, 28), (24, 30), (30, 28), (22, 24)):
        d.point((x, y), fill=BLACK)


def draw_tambor(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((10, 12, 38, 22), outline=BLACK)
    d.rectangle((10, 17, 38, 34), outline=BLACK)
    d.ellipse((10, 28, 38, 38), outline=BLACK)
    d.line((14, 10, 8, 6), fill=BLACK)
    d.line((34, 10, 40, 6), fill=BLACK)


def draw_camaron(d: ImageDraw.ImageDraw) -> None:
    d.arc((10, 16, 38, 40), 200, 20, fill=BLACK)
    d.arc((14, 18, 34, 36), 200, 20, fill=BLACK)
    d.ellipse((32, 14, 40, 22), outline=BLACK)
    d.line((36, 14, 42, 10), fill=BLACK)
    d.line((18, 34, 14, 42), fill=BLACK)
    d.line((24, 36, 24, 44), fill=BLACK)


def draw_jaras(d: ImageDraw.ImageDraw) -> None:
    for dx in (-6, 0, 6):
        d.line((24 + dx, 10, 24 + dx, 38), fill=BLACK)
        d.polygon([(24 + dx, 8), (20 + dx, 14), (28 + dx, 14)], outline=BLACK)
        d.line((20 + dx, 38, 28 + dx, 38), fill=BLACK)


def draw_musico(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((16, 8, 28, 20), outline=BLACK)
    d.polygon([(14, 20), (30, 20), (32, 36), (12, 36)], outline=BLACK)
    d.ellipse((30, 22, 42, 34), outline=BLACK)
    d.line((36, 22, 36, 16), fill=BLACK)
    d.line((18, 36, 18, 44), fill=BLACK)
    d.line((26, 36, 26, 44), fill=BLACK)


def draw_arana(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((18, 18, 30, 30), outline=BLACK)
    for angle in range(0, 360, 45):
        rad = math.radians(angle)
        x0 = 24 + int(6 * math.cos(rad))
        y0 = 24 + int(6 * math.sin(rad))
        x1 = 24 + int(18 * math.cos(rad))
        y1 = 24 + int(18 * math.sin(rad))
        d.line((x0, y0, x1, y1), fill=BLACK)
    d.ellipse((21, 21, 23, 23), fill=BLACK)
    d.ellipse((25, 21, 27, 23), fill=BLACK)


def draw_soldado(d: ImageDraw.ImageDraw) -> None:
    d.rectangle((16, 6, 32, 12), outline=BLACK)
    d.ellipse((18, 12, 30, 24), outline=BLACK)
    d.polygon([(16, 24), (32, 24), (34, 38), (14, 38)], outline=BLACK)
    d.line((14, 28, 10, 34), fill=BLACK)
    d.line((34, 28, 38, 20), fill=BLACK)
    d.line((38, 20, 38, 36), fill=BLACK)
    d.line((20, 38, 20, 44), fill=BLACK)
    d.line((28, 38, 28, 44), fill=BLACK)


def draw_estrella(d: ImageDraw.ImageDraw) -> None:
    cx, cy, r_out, r_in = 24, 24, 16, 7
    pts = []
    for i in range(10):
        r = r_out if i % 2 == 0 else r_in
        a = math.radians(-90 + i * 36)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    d.polygon(pts, outline=BLACK)


def draw_cazo(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((10, 16, 34, 40), outline=BLACK)
    d.arc((10, 12, 34, 28), 200, 340, fill=BLACK)
    d.line((34, 24, 42, 16), fill=BLACK)
    d.line((42, 16, 40, 14), fill=BLACK)


def draw_mundo(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((10, 10, 38, 38), outline=BLACK)
    d.ellipse((18, 10, 30, 38), outline=BLACK)
    d.arc((10, 16, 38, 24), 0, 180, fill=BLACK)
    d.arc((10, 26, 38, 34), 0, 180, fill=BLACK)


def draw_apache(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((16, 16, 32, 32), outline=BLACK)
    for i, h in enumerate((8, 6, 10, 7, 9)):
        x = 16 + i * 4
        d.line((x, 16, x, 16 - h), fill=BLACK)
        d.ellipse((x - 1, 16 - h - 2, x + 1, 16 - h), fill=BLACK)
    d.ellipse((20, 22, 22, 24), fill=BLACK)
    d.ellipse((26, 22, 28, 24), fill=BLACK)
    d.polygon([(16, 32), (32, 32), (34, 44), (14, 44)], outline=BLACK)


def draw_nopal(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((16, 20, 32, 40), outline=BLACK)
    d.ellipse((8, 14, 20, 28), outline=BLACK)
    d.ellipse((28, 12, 40, 26), outline=BLACK)
    for x, y in ((20, 24), (24, 30), (28, 24), (14, 20), (34, 18)):
        d.line((x, y, x + 2, y - 2), fill=BLACK)


def draw_alacran(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((14, 20, 34, 32), outline=BLACK)
    d.arc((28, 8, 42, 28), 90, 270, fill=BLACK)
    d.ellipse((36, 8, 42, 14), outline=BLACK)
    for y in (22, 26, 30):
        d.line((14, y, 8, y - 4), fill=BLACK)
        d.line((34, y, 40, y - 4), fill=BLACK)
    d.line((16, 32, 12, 40), fill=BLACK)
    d.line((20, 32, 18, 42), fill=BLACK)


def draw_rosa(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((16, 12, 32, 28), outline=BLACK)
    d.ellipse((18, 14, 30, 26), outline=BLACK)
    d.ellipse((21, 17, 27, 23), outline=BLACK)
    d.line((24, 28, 24, 40), fill=BLACK)
    d.polygon([(24, 32), (14, 28), (18, 36)], outline=BLACK)
    d.polygon([(24, 34), (34, 30), (30, 38)], outline=BLACK)


def draw_calavera(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((12, 10, 36, 34), outline=BLACK)
    d.ellipse((16, 18, 22, 26), fill=BLACK)
    d.ellipse((26, 18, 32, 26), fill=BLACK)
    d.polygon([(22, 26), (26, 26), (24, 30)], fill=BLACK)
    d.line((18, 32, 30, 32), fill=BLACK)
    d.rectangle((18, 34, 30, 40), outline=BLACK)


def draw_campana(d: ImageDraw.ImageDraw) -> None:
    d.polygon([(12, 30), (36, 30), (32, 14), (16, 14)], outline=BLACK)
    d.ellipse((20, 8, 28, 16), outline=BLACK)
    d.ellipse((22, 30, 26, 36), fill=BLACK)
    d.arc((12, 28, 36, 36), 0, 180, fill=BLACK)


def draw_cantarito(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((16, 10, 32, 18), outline=BLACK)
    d.polygon([(14, 16), (34, 16), (32, 40), (16, 40)], outline=BLACK)
    d.arc((30, 20, 40, 34), 270, 90, fill=BLACK)
    d.line((18, 24, 30, 24), fill=BLACK)


def draw_venado(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((16, 18, 34, 34), outline=BLACK)
    d.ellipse((30, 12, 40, 22), outline=BLACK)
    d.line((34, 12, 30, 4), fill=BLACK)
    d.line((30, 4, 28, 10), fill=BLACK)
    d.line((36, 12, 40, 4), fill=BLACK)
    d.line((40, 4, 42, 10), fill=BLACK)
    d.line((20, 34, 18, 44), fill=BLACK)
    d.line((30, 34, 32, 44), fill=BLACK)


def draw_sol(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((16, 16, 32, 32), outline=BLACK)
    for i in range(12):
        a = math.radians(i * 30)
        x0 = 24 + int(10 * math.cos(a))
        y0 = 24 + int(10 * math.sin(a))
        x1 = 24 + int(18 * math.cos(a))
        y1 = 24 + int(18 * math.sin(a))
        d.line((x0, y0, x1, y1), fill=BLACK)


def draw_corona(d: ImageDraw.ImageDraw) -> None:
    d.polygon([(10, 30), (14, 14), (20, 24), (24, 10), (28, 24), (34, 14), (38, 30)], outline=BLACK)
    d.rectangle((10, 30, 38, 36), outline=BLACK)
    d.ellipse((22, 18, 26, 22), fill=BLACK)


def draw_chalupa(d: ImageDraw.ImageDraw) -> None:
    d.arc((8, 24, 40, 42), 200, 340, fill=BLACK)
    d.polygon([(12, 28), (36, 28), (32, 36), (16, 36)], outline=BLACK)
    d.line((24, 12, 24, 28), fill=BLACK)
    d.polygon([(24, 12), (24, 22), (34, 22)], outline=BLACK)


def draw_pino(d: ImageDraw.ImageDraw) -> None:
    d.polygon([(24, 6), (12, 20), (36, 20)], outline=BLACK)
    d.polygon([(24, 14), (10, 30), (38, 30)], outline=BLACK)
    d.polygon([(24, 22), (8, 38), (40, 38)], outline=BLACK)
    d.rectangle((22, 38, 26, 44), outline=BLACK)


def draw_pescado(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((10, 16, 34, 34), outline=BLACK)
    d.polygon([(34, 20), (44, 24), (34, 30)], outline=BLACK)
    d.ellipse((16, 22, 20, 26), fill=BLACK)
    d.polygon([(12, 18), (8, 12), (14, 20)], outline=BLACK)


def draw_palma(d: ImageDraw.ImageDraw) -> None:
    d.line((24, 18, 24, 42), fill=BLACK)
    for a, r in ((-50, 16), (-20, 18), (20, 18), (50, 16)):
        rad = math.radians(a - 90)
        d.line((24, 18, 24 + int(r * math.cos(rad)), 18 + int(r * math.sin(rad))), fill=BLACK)
    d.ellipse((20, 40, 28, 46), outline=BLACK)


def draw_maceta(d: ImageDraw.ImageDraw) -> None:
    d.polygon([(14, 24), (34, 24), (30, 42), (18, 42)], outline=BLACK)
    d.rectangle((12, 20, 36, 26), outline=BLACK)
    d.ellipse((20, 8, 28, 18), outline=BLACK)
    d.line((24, 18, 24, 22), fill=BLACK)
    d.polygon([(24, 12), (16, 10), (20, 16)], outline=BLACK)


def draw_arpa(d: ImageDraw.ImageDraw) -> None:
    d.arc((10, 8, 38, 42), 270, 90, fill=BLACK)
    d.line((24, 8, 24, 42), fill=BLACK)
    for y in range(12, 40, 4):
        d.line((24, y, 34, y + 2), fill=BLACK)


def draw_rana(d: ImageDraw.ImageDraw) -> None:
    d.ellipse((12, 18, 36, 36), outline=BLACK)
    d.ellipse((16, 12, 24, 20), outline=BLACK)
    d.ellipse((24, 12, 32, 20), outline=BLACK)
    d.ellipse((18, 14, 20, 16), fill=BLACK)
    d.ellipse((28, 14, 30, 16), fill=BLACK)
    d.arc((18, 24, 30, 32), 20, 160, fill=BLACK)
    d.line((12, 28, 6, 34), fill=BLACK)
    d.line((36, 28, 42, 34), fill=BLACK)


DRAWERS = {
    "el_gallo": draw_gallo,
    "el_diablito": draw_diablito,
    "la_dama": draw_dama,
    "el_catrin": draw_catrin,
    "el_paraguas": draw_paraguas,
    "la_sirena": draw_sirena,
    "la_escalera": draw_escalera,
    "la_botella": draw_botella,
    "el_barril": draw_barril,
    "el_arbol": draw_arbol,
    "el_melon": draw_melon,
    "el_valiente": draw_valiente,
    "el_gorrito": draw_gorrito,
    "la_muerte": draw_muerte,
    "la_pera": draw_pera,
    "la_bandera": draw_bandera,
    "el_bandolon": draw_bandolon,
    "el_violoncello": draw_violoncello,
    "la_garza": draw_garza,
    "el_pajaro": draw_pajaro,
    "la_mano": draw_mano,
    "la_bota": draw_bota,
    "la_luna": draw_luna,
    "el_cotorro": draw_cotorro,
    "el_borracho": draw_borracho,
    "el_negrito": draw_negrito,
    "el_corazon": draw_corazon,
    "la_sandia": draw_sandia,
    "el_tambor": draw_tambor,
    "el_camaron": draw_camaron,
    "las_jaras": draw_jaras,
    "el_musico": draw_musico,
    "la_arana": draw_arana,
    "el_soldado": draw_soldado,
    "la_estrella": draw_estrella,
    "el_cazo": draw_cazo,
    "el_mundo": draw_mundo,
    "el_apache": draw_apache,
    "el_nopal": draw_nopal,
    "el_alacran": draw_alacran,
    "la_rosa": draw_rosa,
    "la_calavera": draw_calavera,
    "la_campana": draw_campana,
    "el_cantarito": draw_cantarito,
    "el_venado": draw_venado,
    "el_sol": draw_sol,
    "la_corona": draw_corona,
    "la_chalupa": draw_chalupa,
    "el_pino": draw_pino,
    "el_pescado": draw_pescado,
    "la_palma": draw_palma,
    "la_maceta": draw_maceta,
    "el_arpa": draw_arpa,
    "la_rana": draw_rana,
}


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    catalog = json.loads((root / "cards.json").read_text(encoding="utf-8"))
    out_dir = root / "images"
    missing = []
    for card in catalog["cards"]:
        key = card["image"]
        drawer = DRAWERS.get(key)
        if drawer is None:
            missing.append(key)
            continue
        img, draw = new_canvas()
        drawer(draw)
        save(img, out_dir / f"{key}.png")
    if missing:
        raise SystemExit(f"No drawer for: {missing}")
    print(f"Generated {len(catalog['cards'])} card illustrations in {out_dir}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Promo art, built from the game's own screens rather than around them.

The device panel in each image is a real rendered frame - the same
pixels shoot.c writes into renders/ - so the art cannot drift from what
the game looks like. Palette sampled from the browser build's
existing cover so the two sit together.
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

BG = (28, 28, 30)
ORANGE = (255, 130, 0)
INK = (26, 22, 18)
BEZEL = (168, 92, 20)
GREY = (150, 150, 155)

BOLD = "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"
REG = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"

# the five button shapes, in the game's own order: UP DOWN LEFT RIGHT OK
SHAPES = ("star", "circle", "triangle", "pentagon", "square")


def poly(draw, cx, cy, r, sides, rotation, fill):
    import math

    pts = [
        (
            cx + r * math.cos(math.radians(rotation + i * 360 / sides)),
            cy + r * math.sin(math.radians(rotation + i * 360 / sides)),
        )
        for i in range(sides)
    ]
    draw.polygon(pts, fill=fill)


def star(draw, cx, cy, r, fill):
    import math

    pts = []
    for i in range(10):
        rad = r if i % 2 == 0 else r * 0.42
        a = math.radians(-90 + i * 36)
        pts.append((cx + rad * math.cos(a), cy + rad * math.sin(a)))
    draw.polygon(pts, fill=fill)


def shape(draw, kind, cx, cy, r, fill):
    if kind == "circle":
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=fill)
    elif kind == "square":
        s = r * 0.82
        draw.rectangle([cx - s, cy - s, cx + s, cy + s], fill=fill)
    elif kind == "triangle":
        poly(draw, cx, cy + r * 0.12, r * 1.12, 3, -90, fill)
    elif kind == "pentagon":
        poly(draw, cx, cy, r * 1.05, 5, -90, fill)
    else:
        star(draw, cx, cy, r * 1.15, fill)


def panel(img, screen_png, x, y, w):
    """the device screen, at its own aspect, with the Flipper's bezel"""
    shot = Image.open(screen_png).convert("RGB")
    h = round(w * shot.height / shot.width)
    shot = shot.resize((w, h), Image.Resampling.NEAREST)
    pad = max(6, w // 40)
    frame = Image.new("RGB", (w + pad * 2, h + pad * 2), BEZEL)
    d = ImageDraw.Draw(frame)
    d.rounded_rectangle(
        [0, 0, frame.width - 1, frame.height - 1], radius=pad, fill=BEZEL
    )
    frame.paste(shot, (pad, pad))
    img.paste(frame, (x, y))
    return frame.width, frame.height


def cover(shots, out):
    img = Image.new("RGB", (630, 500), BG)
    d = ImageDraw.Draw(img)
    for i, kind in enumerate(SHAPES):
        shape(d, kind, 91 + i * 112, 88, 44, ORANGE)
    d.text(
        (315, 212),
        "BEEPBACK",
        font=ImageFont.truetype(BOLD, 82),
        fill=ORANGE,
        anchor="mm",
    )
    d.text(
        (315, 272),
        "listen  ·  watch  ·  repeat",
        font=ImageFont.truetype(REG, 21),
        fill=GREY,
        anchor="mm",
    )
    w, h = panel(img, shots / "ss0.png", 0, 0, 300)
    img = Image.new("RGB", (630, 500), BG)
    d = ImageDraw.Draw(img)
    for i, kind in enumerate(SHAPES):
        shape(d, kind, 91 + i * 112, 88, 44, ORANGE)
    d.text(
        (315, 212),
        "BEEPBACK",
        font=ImageFont.truetype(BOLD, 82),
        fill=ORANGE,
        anchor="mm",
    )
    d.text(
        (315, 272),
        "listen  ·  watch  ·  repeat",
        font=ImageFont.truetype(REG, 21),
        fill=GREY,
        anchor="mm",
    )
    panel(img, shots / "ss0.png", (630 - w) // 2, 500 - h - 30, 300)
    img.save(out / "cover.png")
    print("cover.png  630x500")


def banner(shots, out):
    img = Image.new("RGB", (1200, 400), BG)
    d = ImageDraw.Draw(img)
    d.text(
        (80, 150),
        "BEEPBACK",
        font=ImageFont.truetype(BOLD, 88),
        fill=ORANGE,
        anchor="lm",
    )
    d.text(
        (84, 215),
        "listen  ·  watch  ·  repeat",
        font=ImageFont.truetype(REG, 24),
        fill=GREY,
        anchor="lm",
    )
    for i, kind in enumerate(SHAPES):
        shape(d, kind, 100 + i * 66, 295, 22, BEZEL)
    panel(img, shots / "ss0.png", 700, 90, 420)
    img.save(out / "banner.png")
    print("banner.png  1200x400")


def strip(shots, out):
    """all four catalog screenshots in a row, for a README or a post"""
    names = ["ss0.png", "ss1.png", "ss2.png", "ss3.png"]
    gap, pad = 24, 40
    tile_w = 260
    img = Image.new("RGB", (pad * 2 + tile_w * 4 + gap * 3 + 4 * 14, 260), BG)
    x = pad
    for n in names:
        w, h = panel(img, shots / n, x, (260 - round(tile_w * 0.5) - 28) // 2, tile_w)
        x += w + gap
    img = img.crop((0, 0, x - gap + pad, 260))
    img.save(out / "strip.png")
    print(f"strip.png  {img.width}x{img.height}")


def main():
    root = Path(__file__).resolve().parents[2]
    shots, out = root / "renders", root / "promo"
    out.mkdir(exist_ok=True)
    cover(shots, out)
    banner(shots, out)
    strip(shots, out)
    return 0


if __name__ == "__main__":
    sys.exit(main())

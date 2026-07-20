#!/usr/bin/env python3
"""
Generate FlipDeFlock branding:
  - icon.png  : 10x10 1-bit Flipper .fap menu icon (camera-with-slash glyph)
  - assets/logo.png : repo/README banner (camera + slash + wordmark)

Run:  python assets/make_logo.py
Requires Pillow.
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# ---- 10x10 .fap icon -----------------------------------------------------
# Hand-pixeled so it stays crisp at 10x10. '#' = black (drawn), '.' = clear.
# A small surveillance camera (body + lens + mount) with a diagonal slash
# across it == "detect / de-flock the cameras".
ICON = [
    "...###....",
    ".##...#...",
    ".##....#..",
    "#..#....#.",
    "#...#...#.",
    "#....#..#.",
    ".#....##..",
    "..#...##..",
    "...###....",
    "..........",
]


def build_icon():
    img = Image.new("1", (10, 10), 1)  # 1 = white background
    px = img.load()
    for y, row in enumerate(ICON):
        for x, c in enumerate(row):
            if c == "#":
                px[x, y] = 0  # black
    img.save(os.path.join(ROOT, "icon.png"))
    # ASCII preview to stdout for sanity.
    print("icon.png (10x10):")
    print("\n".join(ICON))


# ---- README banner logo --------------------------------------------------
NAVY = (13, 27, 42)
WHITE = (236, 240, 241)
GREY = (149, 165, 166)
ORANGE = (255, 130, 0)  # Flipper accent
RED = (231, 76, 60)


def build_logo():
    W, H = 880, 260
    img = Image.new("RGB", (W, H), NAVY)
    d = ImageDraw.Draw(img)

    # --- emblem: a CCTV camera with a slash, drawn big on the left ---
    cx, cy = 130, 130
    # camera body (rounded rect)
    d.rounded_rectangle([cx - 70, cy - 38, cx + 50, cy + 38], radius=10,
                        outline=WHITE, width=7)
    # lens
    d.ellipse([cx - 34, cy - 22, cx + 10, cy + 22], outline=WHITE, width=7)
    d.ellipse([cx - 18, cy - 6, cx - 6, cy + 6], fill=WHITE)
    # barrel/front toward right
    d.polygon([(cx + 50, cy - 18), (cx + 78, cy - 30),
               (cx + 78, cy + 30), (cx + 50, cy + 18)], outline=WHITE, width=7)
    # mount stem + base
    d.rectangle([cx - 8, cy + 38, cx + 8, cy + 60], fill=WHITE)
    d.rectangle([cx - 34, cy + 60, cx + 34, cy + 72], fill=WHITE)
    # bold red anti-surveillance slash
    d.line([(cx - 92, cy + 86), (cx + 96, cy - 86)], fill=RED, width=14)

    # --- wordmark ---
    try:
        bold = ImageFont.truetype("C:/Windows/Fonts/arialbd.ttf", 78)
        sub = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 30)
    except Exception:
        bold = ImageFont.load_default()
        sub = ImageFont.load_default()

    tx = 250
    # "Flip" white, "De" orange, "Flock" white
    parts = [("Flip", WHITE), ("De", ORANGE), ("Flock", WHITE)]
    x = tx
    for text, color in parts:
        d.text((x, 70), text, font=bold, fill=color)
        x += d.textlength(text, font=bold)
    d.text((tx + 2, 162), "Flock / ALPR detection for Flipper Zero",
           font=sub, fill=GREY)

    img.save(os.path.join(HERE, "logo.png"))
    print("logo.png:", img.size)


if __name__ == "__main__":
    build_icon()
    build_logo()

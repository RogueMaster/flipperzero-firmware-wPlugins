#!/usr/bin/env python3
"""Render 128x64 Flipper-style mockups of RollCall's screens for the README.
Amber LCD look, dark ink, upscaled NEAREST for a crisp pixel feel + a bezel.
These mirror the real views in views/capture_view.c and views/verdict_view.c."""
from PIL import Image, ImageDraw, ImageFont
import os

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

ORANGE = (255, 159, 12)
INK = (26, 18, 2)
BEZEL = (18, 18, 22)
BEZEL_HI = (44, 44, 52)

SCALE = 7
W, H = 128, 64

FB = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
FR = "/System/Library/Fonts/Supplemental/Arial.ttf"
FBLK = "/System/Library/Fonts/Supplemental/Arial Black.ttf"


def f(path, px):
    return ImageFont.truetype(path, px)


PRIM = f(FB, 9)
SEC = f(FR, 8)
BIG = f(FBLK, 20)
BADGE = f(FBLK, 15)


def screen():
    return Image.new("RGB", (W, H), ORANGE)


def ctext(d, cx, y, s, font, fill=INK):
    w = d.textlength(s, font=font)
    d.text((cx - w / 2, y), s, font=font, fill=fill)


def rtext(d, rx, y, s, font, fill=INK):
    w = d.textlength(s, font=font)
    d.text((rx - w, y), s, font=font, fill=fill)


def finish(img, name):
    up = img.resize((W * SCALE, H * SCALE), Image.NEAREST)
    pad = 20
    canvas = Image.new("RGB", (W * SCALE + pad * 2, H * SCALE + pad * 2), BEZEL)
    d = ImageDraw.Draw(canvas)
    d.rounded_rectangle([6, 6, canvas.width - 6, canvas.height - 6], radius=16,
                        outline=BEZEL_HI, width=3)
    canvas.paste(up, (pad, pad))
    path = os.path.join(OUT, name)
    canvas.save(path)
    print("wrote", path)
    return path


# ----------------------------------------------------------------- 1. menu
def m_menu():
    img = screen()
    d = ImageDraw.Draw(img)
    ctext(d, 64, 0, "RollCall", PRIM)
    d.line([(0, 12), (128, 12)], fill=INK)
    d.rounded_rectangle([2, 16, 125, 30], radius=3, fill=INK)
    d.text((7, 18), "Run Health Check", font=SEC, fill=ORANGE)
    d.text((7, 34), "Settings", font=SEC, fill=INK)
    d.text((7, 48), "How it works", font=SEC, fill=INK)
    return finish(img, "screen_menu.png")


# ----------------------------------------------------------------- 2. capture
def m_capture():
    img = screen()
    d = ImageDraw.Draw(img)
    d.text((2, 1), "RollCall", font=PRIM, fill=INK)
    rtext(d, 126, 1, "433.92 AM", SEC)
    d.line([(0, 12), (128, 12)], fill=INK)
    # antenna + rings
    cx, cy = 22, 31
    for r in (6, 11, 16):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=INK, width=1)
    d.line([(cx, cy), (cx, cy - 12)], fill=INK, width=1)
    d.ellipse([cx - 2, cy - 14, cx + 2, cy - 10], fill=INK)
    d.line([(cx - 4, cy + 4), (cx + 4, cy + 4)], fill=INK, width=1)
    # big counter
    d.text((58, 16), "2", font=BIG, fill=INK)
    bw = int(d.textlength("2", font=BIG))
    d.text((58 + bw + 2, 20), "/3", font=SEC, fill=INK)
    d.text((58, 40), "presses", font=SEC, fill=INK)
    # slot dots (2 filled of 3)
    sx = 64 - (3 * 8 - 3) // 2
    for i in range(3):
        x = sx + i * 8
        if i < 2:
            d.ellipse([x - 2, 50, x + 2, 54], fill=INK)
        else:
            d.ellipse([x - 2, 50, x + 2, 54], outline=INK, width=1)
    d.text((2, 40), "> KeeLoq", font=SEC, fill=INK)
    # footer
    d.rectangle([0, 57, 128, 64], fill=INK)
    d.text((3, 56), "Press remote..", font=SEC, fill=ORANGE)
    rtext(d, 125, 56, "OK: Analyze", SEC, fill=ORANGE)
    return finish(img, "screen_capture.png")


# ----------------------------------------------------------------- 3. verdict
def m_verdict():
    img = screen()
    d = ImageDraw.Draw(img)
    d.text((2, 1), "KeeLoq", font=PRIM, fill=INK)
    d.line([(0, 12), (128, 12)], fill=INK)
    # badge
    d.rounded_rectangle([2, 15, 36, 49], radius=3, outline=INK, width=1)
    ctext(d, 19, 20, "A", BADGE)
    ctext(d, 19, 39, "Rolling", SEC)
    # headline (2 lines)
    d.text((40, 15), "Rolling code", font=SEC, fill=INK)
    d.text((40, 24), "confirmed", font=SEC, fill=INK)
    # meter (near full) + segment ticks
    d.rectangle([40, 35, 126, 43], outline=INK, width=1)
    d.rectangle([42, 37, 42 + int(82 * 1.0) - 4, 41], fill=INK)
    for x in range(52, 126, 12):
        d.line([(x, 35), (x, 42)], fill=ORANGE, width=1)
    # tally
    d.text((40, 46), "3 presses . 3 codes", font=SEC, fill=INK)
    # footer
    d.rectangle([0, 57, 128, 64], fill=INK)
    d.text((3, 56), "OK: Details", font=SEC, fill=ORANGE)
    rtext(d, 125, 56, "Retest >", SEC, fill=ORANGE)
    return finish(img, "screen_verdict.png")


# ----------------------------------------------------------------- 4. details
def m_details():
    img = screen()
    d = ImageDraw.Draw(img)
    d.text((2, 0), "KeeLoq", font=PRIM, fill=INK)
    d.line([(0, 11), (128, 11)], fill=INK)
    rows = [
        "Grade A  Rolling  HEALTHY",
        "Every press produced a",
        "DIFFERENT parcel (3",
        "presses, 3 unique codes).",
        "Presses seen",
        "1. KeeLoq 9A3F.. (new)",
    ]
    y = 13
    for r in rows:
        d.text((2, y), r, font=SEC, fill=INK)
        y += 8
    d.rectangle([124, 12, 127, 63], outline=INK, width=1)
    d.rectangle([124, 12, 127, 28], fill=INK)
    return finish(img, "screen_details.png")


def strip(paths):
    imgs = [Image.open(p) for p in paths]
    gap = 24
    tw = sum(i.width for i in imgs) + gap * (len(imgs) - 1)
    th = max(i.height for i in imgs)
    canvas = Image.new("RGB", (tw, th), (13, 15, 22))
    x = 0
    for im in imgs:
        canvas.paste(im, (x, (th - im.height) // 2))
        x += im.width + gap
    path = os.path.join(OUT, "screens.png")
    canvas.save(path)
    print("wrote", path)


if __name__ == "__main__":
    p = [m_menu(), m_capture(), m_verdict(), m_details()]
    strip(p)

#!/usr/bin/env python3
"""Render 128x64 Flipper-style mockups of Gauntlet's screens for the README.
Amber LCD look, dark ink, upscaled NEAREST for a crisp pixel feel + a bezel.
These mirror the real views in views/challenge_view.c, toolkit_view.c and
result_view.c."""
from PIL import Image, ImageDraw, ImageFont
import os, math

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
BIG = f(FBLK, 11)


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


def header(d, tag, pips, pts):
    """The inverted challenge header: category tag, difficulty pips, points."""
    d.rectangle([0, 0, 128, 10], fill=INK)
    d.text((3, 1), tag, font=SEC, fill=ORANGE)
    px = 64 - (4 * 4 + 3 * 2) // 2
    for i in range(4):
        x = px + i * 6
        if i < pips:
            d.rectangle([x, 4, x + 3, 7], fill=ORANGE)
        else:
            d.rectangle([x, 4, x + 3, 7], outline=ORANGE, width=1)
    rtext(d, 125, 1, pts, SEC, fill=ORANGE)


# ------------------------------------------------------------------- 1. menu
def m_menu():
    img = screen()
    d = ImageDraw.Draw(img)
    ctext(d, 64, 0, "Gauntlet", PRIM)
    d.line([(0, 12), (128, 12)], fill=INK)
    d.rounded_rectangle([2, 15, 125, 27], radius=3, fill=INK)
    d.text((6, 17), "Play", font=SEC, fill=ORANGE)
    d.text((6, 30), "Toolkit", font=SEC, fill=INK)
    d.text((6, 41), "Progress", font=SEC, fill=INK)
    d.text((6, 52), "Settings", font=SEC, fill=INK)
    return finish(img, "screen_menu.png")


# -------------------------------------------------------------- 2. challenge
def m_challenge():
    img = screen()
    d = ImageDraw.Draw(img)
    header(d, "CIPHER", 1, "100p")
    d.text((2, 13), "Caesar's Salad", font=PRIM, fill=INK)
    d.text((2, 27), "A classic shift cipher", font=SEC, fill=INK)
    d.text((2, 36), ">> DATA", font=SEC, fill=INK)
    d.text((2, 45), "WKH NHB LV EUXWXV", font=SEC, fill=INK)
    d.rectangle([0, 56, 128, 64], fill=INK)
    d.text((2, 56), "OK Flag", font=SEC, fill=ORANGE)
    ctext(d, 68, 56, "<Hint", SEC, fill=ORANGE)
    rtext(d, 125, 56, "Tools>", SEC, fill=ORANGE)
    return finish(img, "screen_challenge.png")


# ---------------------------------------------------------------- 3. toolkit
def m_toolkit():
    img = screen()
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 128, 10], fill=INK)
    d.text((3, 1), "<", font=SEC, fill=ORANGE)
    rtext(d, 124, 1, ">", SEC, fill=ORANGE)
    ctext(d, 64, 1, "ROT-23", SEC, fill=ORANGE)
    d.text((2, 13), "in: WKH NHB LV EUXWXV", font=SEC, fill=INK)
    d.line([(0, 23), (128, 23)], fill=INK)
    d.text((2, 30), "THE KEY IS BRUTUS", font=SEC, fill=INK)
    d.rectangle([0, 56, 128, 64], fill=INK)
    d.text((2, 56), "UpDn Shift", font=SEC, fill=ORANGE)
    rtext(d, 125, 56, "OK Use", SEC, fill=ORANGE)
    return finish(img, "screen_toolkit.png")


# ----------------------------------------------------------------- 4. result
def m_result():
    img = screen()
    d = ImageDraw.Draw(img)
    cx, cy = 64, 18
    for i in range(12):
        a = i * (2 * math.pi / 12)
        d.line([(cx + math.cos(a) * 15, cy + math.sin(a) * 15),
                (cx + math.cos(a) * 26, cy + math.sin(a) * 26)], fill=INK, width=1)
    # flag on a pole
    d.line([(cx - 2, cy - 9), (cx - 2, cy + 9)], fill=INK, width=2)
    for i in range(8):
        d.line([(cx, cy - 9 + i), (cx + (8 - i), cy - 9 + i)], fill=INK, width=1)
    ctext(d, 64, 33, "FLAG CAPTURED", PRIM)
    ctext(d, 64, 45, "+100 pts   Score 100", SEC)
    d.rectangle([0, 56, 128, 64], fill=INK)
    ctext(d, 64, 56, "Rank: Initiate", SEC, fill=ORANGE)
    return finish(img, "screen_result.png")


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
    p = [m_menu(), m_challenge(), m_toolkit(), m_result()]
    strip(p)

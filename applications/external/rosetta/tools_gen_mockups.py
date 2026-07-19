#!/usr/bin/env python3
"""Render 128x64 Flipper-style mockups of Rosetta's screens for the README.
Amber LCD look, dark ink, upscaled NEAREST for a crisp pixel feel + a bezel."""
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
MONO = f(FR, 8)


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
    ctext(d, 64, 0, "Rosetta", PRIM)
    d.line([(0, 12), (128, 12)], fill=INK)
    items = ["Mifare Auth", "OOK & PSK", "1-Wire", "Settings"]
    y = 15
    for i, it in enumerate(items):
        if i == 0:
            d.rounded_rectangle([2, y - 1, 125, y + 11], radius=3, fill=INK)
            d.text((7, y + 1), it, font=SEC, fill=ORANGE)
        else:
            d.text((7, y + 1), it, font=SEC, fill=INK)
        y += 12
    return finish(img, "screen_menu.png")


# -------------------------------------------------- 2. walkthrough (Crypto1)
def m_lesson():
    img = screen()
    d = ImageDraw.Draw(img)
    # title bar
    d.rectangle([0, 0, 128, 12], fill=INK)
    d.text((3, 1), "3-Pass Crypto1", font=PRIM, fill=ORANGE)
    # step pips
    for i in range(5):
        cx = 124 - (4 - i) * 7
        if i == 3:
            d.ellipse([cx - 2, 4, cx + 2, 8], fill=ORANGE)
        else:
            d.ellipse([cx - 2, 4, cx + 2, 8], outline=ORANGE, width=1)
    # actors
    d.rounded_rectangle([2, 20, 32, 36], radius=2, outline=INK, width=1)
    ctext(d, 17, 24, "RDR", SEC)
    d.rounded_rectangle([96, 20, 126, 36], radius=2, outline=INK, width=1)
    ctext(d, 111, 24, "TAG", SEC)
    # message line + packet + label
    ctext(d, 64, 15, "{nR,aR}", SEC)
    d.line([(34, 28), (94, 28)], fill=INK, width=1)
    d.rectangle([60, 26, 64, 30], fill=INK)
    d.line([(94, 28), (91, 26)], fill=INK)
    d.line([(94, 28), (91, 30)], fill=INK)
    # leg pips
    for i in range(3):
        cx = 60 + i * 6
        if i == 1:
            d.ellipse([cx - 2, 40, cx + 2, 44], fill=INK)
        else:
            d.ellipse([cx - 2, 40, cx + 2, 44], outline=INK, width=1)
    # caption
    d.text((2, 47), "Card nT, reader {nR,aR},", font=SEC, fill=INK)
    d.text((2, 55), "card aT.", font=SEC, fill=INK)
    return finish(img, "screen_lesson.png")


# -------------------------------------------------- 3. live capture (iButton)
def m_capture():
    img = screen()
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 128, 13], fill=INK)
    d.text((3, 1), "Captured", font=PRIM, fill=ORANGE)
    rows = [
        "Family 01: DS1990A key",
        "SN A2F19C4B0033",
        "CRC 8C  calc 8C",
    ]
    y = 17
    for r in rows:
        d.text((4, y), r, font=SEC, fill=INK)
        y += 10
    # verdict banner (good = frame)
    d.rounded_rectangle([0, 50, 127, 62], radius=2, outline=INK, width=1)
    d.text((4, 52), "CRC valid: genuine ROM", font=SEC, fill=INK)
    return finish(img, "screen_capture.png")


# -------------------------------------------------- 4. RF envelope scope
def m_scope():
    img = screen()
    d = ImageDraw.Draw(img)
    d.text((2, 2), "433.92 MHz", font=PRIM, fill=INK)
    rtext(d, 126, 3, "-58 dBm", SEC)
    # plot frame baseline
    y1 = 52
    d.line([(0, y1), (127, y1)], fill=INK)
    # threshold (dotted)
    ty = 30
    for x in range(0, 128, 3):
        d.point((x, ty), fill=INK)
    # envelope: noise floor with a couple of OOK bursts
    import random
    random.seed(7)
    pattern = []
    x = 0
    while x < 128:
        burst = random.random() < 0.22
        wdt = random.randint(4, 12)
        for i in range(wdt):
            if x + i < 128:
                pattern.append(38 if burst else 46 + random.randint(-1, 1))
        x += wdt
    for x in range(1, min(128, len(pattern))):
        y = pattern[x]
        d.point((x, y), fill=INK)
        if y <= ty:  # burst above threshold -> stem
            d.line([(x, y), (x, y1 - 1)], fill=INK)
    d.text((2, 55), "OOK: above line = carrier ON", font=SEC, fill=INK)
    return finish(img, "screen_scope.png")


def strip(paths, name="screens.png"):
    imgs = [Image.open(p) for p in paths]
    gap = 24
    tw = sum(i.width for i in imgs) + gap * (len(imgs) - 1)
    th = max(i.height for i in imgs)
    canvas = Image.new("RGB", (tw, th), (13, 15, 22))
    x = 0
    for im in imgs:
        canvas.paste(im, (x, (th - im.height) // 2))
        x += im.width + gap
    path = os.path.join(OUT, name)
    canvas.save(path)
    print("wrote", path)


if __name__ == "__main__":
    menu = m_menu()
    lesson = m_lesson()
    capture = m_capture()
    scope = m_scope()
    strip([menu, lesson, capture, scope])

#!/usr/bin/env python3
"""Render 128x64 Flipper-style mockups of Warden's screens for the README.
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
BIG = f(FBLK, 20)
BADGE = f(FBLK, 15)


def screen():
    return Image.new("RGB", (W, H), ORANGE)


def ctext(d, cx, y, s, font, fill=INK):
    w = d.textlength(s, font=font)
    d.text((cx - w / 2, y), s, font=font, fill=fill)


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
    ctext(d, 64, 0, "Warden", PRIM)
    d.line([(0, 12), (128, 12)], fill=INK)
    # selected item
    d.rounded_rectangle([2, 16, 125, 30], radius=3, fill=INK)
    d.text((7, 18), "Grade a Card", font=SEC, fill=ORANGE)
    d.text((7, 34), "Settings", font=SEC, fill=INK)
    d.text((7, 48), "About", font=SEC, fill=INK)
    return finish(img, "screen_menu.png")


# ----------------------------------------------------------------- 2. scan
def m_scan():
    img = screen()
    d = ImageDraw.Draw(img)
    ctext(d, 64, 0, "Grade a Card", PRIM)
    cx, cy = 64, 29
    for r in (8, 13, 18):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=INK, width=1)
    d.ellipse([cx - 2, cy - 2, cx + 2, cy + 2], fill=INK)
    for dx in (-8, 5):
        d.line([(cx + dx, cy), (cx + dx + 3, cy)], fill=INK, width=1)
    for dy in (-8, 5):
        d.line([(cx, cy + dy), (cx, cy + dy + 3)], fill=INK, width=1)
    ctext(d, 64, 47, "Reading NFC...", SEC)
    ctext(d, 64, 55, "Hold card to the back", SEC)
    return finish(img, "screen_scan.png")


# ----------------------------------------------------------------- 3. grade
def m_grade(fname, title, letter, band, score):
    img = screen()
    d = ImageDraw.Draw(img)
    d.text((2, 1), title, font=PRIM, fill=INK)
    d.line([(0, 12), (128, 12)], fill=INK)
    # badge
    d.rounded_rectangle([2, 14, 34, 44], radius=3, outline=INK, width=1)
    ctext(d, 18, 16, letter, BADGE)
    ctext(d, 18, 34, "GRADE", SEC)
    # band bar
    d.rounded_rectangle([38, 14, 126, 25], radius=2, fill=INK)
    ctext(d, 82, 15, band, SEC, fill=ORANGE)
    # score
    d.text((40, 27), str(score), font=BIG, fill=INK)
    d.text((40 + int(d.textlength(str(score), font=BIG)) + 3, 34), "/100", font=SEC, fill=INK)
    # meter
    d.rectangle([2, 50, 125, 55], outline=INK, width=1)
    fillw = int(120 * score / 100)
    if fillw > 0:
        d.rectangle([4, 52, 4 + fillw, 53], fill=INK)
    # footer
    d.rectangle([0, 57, 128, 64], fill=INK)
    d.text((3, 56), "OK Report", font=SEC, fill=ORANGE)
    s = "Rescan >"
    d.text((125 - d.textlength(s, font=SEC), 56), s, font=SEC, fill=ORANGE)
    return finish(img, fname)


# ----------------------------------------------------------------- 4. report
def m_report():
    img = screen()
    d = ImageDraw.Draw(img)
    d.text((2, 0), "Mifare Classic 1K", font=PRIM, fill=INK)
    d.line([(0, 11), (128, 11)], fill=INK)
    rows = [
        "Grade F  18/100  BROKEN",
        "[x] Crypto1 broken since",
        "    2008",
        "[x] Keys recoverable in",
        "    seconds",
        "[!] Ships on default keys",
    ]
    y = 13
    for r in rows:
        d.text((2, y), r, font=SEC, fill=INK)
        y += 8
    # scrollbar
    d.rectangle([124, 12, 127, 63], outline=INK, width=1)
    d.rectangle([124, 12, 127, 30], fill=INK)
    return finish(img, "screen_report.png")


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
    menu = m_menu()
    scan = m_scan()
    classic = m_grade("screen_grade.png", "Mifare Classic 1K", "F", "BROKEN", 18)
    emv = m_grade("screen_emv.png", "Contactless bank card", "A", "SECURE", 88)
    m_report()
    strip([menu, scan, classic, emv])

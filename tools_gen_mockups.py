#!/usr/bin/env python3
"""Render 128x64 Flipper-style mockups of RollCall's screens for the README.
Amber LCD look, dark ink, upscaled NEAREST for a crisp pixel feel + a bezel.

These mirror the real views: coordinates below are copied from the C, and text
is positioned by BASELINE (PIL anchor="ls"/"rs"/"ms") because canvas_draw_str
takes y as the baseline, not the top of the glyph box. Drawing them any other
way produces pretty pictures that quietly disagree with the firmware."""
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


PRIM = f(FB, 9)     # FontPrimary
SEC = f(FR, 8)      # FontSecondary
BIG = f(FBLK, 20)   # FontBigNumbers
BADGE = f(FBLK, 15)


def screen():
    return Image.new("RGB", (W, H), ORANGE)


def ltext(d, x, baseline, s, font, fill=INK):
    """canvas_draw_str(canvas, x, y, s) - y is the baseline."""
    d.text((x, baseline), s, font=font, fill=fill, anchor="ls")


def rtext(d, x, baseline, s, font, fill=INK):
    """canvas_draw_str_aligned(.., AlignRight, AlignBottom)."""
    d.text((x, baseline), s, font=font, fill=fill, anchor="rs")


def ctext(d, cx, baseline, s, font, fill=INK):
    d.text((cx, baseline), s, font=font, fill=fill, anchor="ms")


def footer(d, left, right):
    """The inverted footer bar every action screen shares."""
    d.rectangle([0, 56, 128, 63], fill=INK)
    ltext(d, 3, 63, left, SEC, fill=ORANGE)
    rtext(d, 125, 63, right, SEC, fill=ORANGE)


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
    ctext(d, 64, 10, "RollCall", PRIM)
    d.line([(0, 12), (128, 12)], fill=INK)
    d.rounded_rectangle([2, 15, 125, 27], radius=3, fill=INK)
    ltext(d, 7, 24, "Run Health Check", SEC, fill=ORANGE)
    ltext(d, 7, 37, "Find My Remote", SEC)
    ltext(d, 7, 50, "Settings", SEC)
    ltext(d, 7, 62, "How it works", SEC)
    return finish(img, "screen_menu.png")


# ----------------------------------------------------------------- 2. capture
def m_capture():
    """views/capture_view.c :: capture_view_draw"""
    img = screen()
    d = ImageDraw.Draw(img)

    ltext(d, 2, 10, "RollCall", PRIM)
    rtext(d, 126, 10, "433.92 AM650", SEC)
    d.line([(0, 12), (128, 12)], fill=INK)

    # antenna + expanding rings: cx/cy and the radius set from the C
    cx, cy = 20, 25
    for r in (5, 8, 11):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=INK, width=1)
    d.line([(cx, cy), (cx, cy - 9)], fill=INK, width=1)
    d.ellipse([cx - 2, cy - 11, cx + 2, cy - 7], fill=INK)
    d.line([(cx - 4, cy + 4), (cx + 4, cy + 4)], fill=INK, width=1)

    # big captured/target counter
    ltext(d, 56, 38, "2", BIG)
    bw = int(d.textlength("2", font=BIG))
    ltext(d, 56 + bw + 2, 38, "/3", SEC)

    # last decoded protocol (left) + slot dots (right)
    ltext(d, 2, 46, "KeeLoq 64bit", SEC)
    slots, pitch, count = 3, 6, 2
    sx = 126 - (slots * pitch - (pitch - 4))
    for i in range(slots):
        x = sx + i * pitch
        box = [x - 2, 41, x + 2, 45]
        if i < count:
            d.ellipse(box, fill=INK)
        else:
            d.ellipse(box, outline=INK, width=1)

    # signal row: activity pip + carrier bar + dBm
    d.rectangle([2, 48, 7, 53], fill=INK)          # pip lit = raw pulses seen
    d.rectangle([11, 48, 79, 53], outline=INK, width=1)
    d.rectangle([12, 49, 12 + 47, 52], fill=INK)   # ~-58 dBm on a -100..-40 scale
    rtext(d, 126, 54, "-58dBm", SEC)

    footer(d, "Press remote..", "OK: Analyze")
    return finish(img, "screen_capture.png")


# ----------------------------------------------------------------- 3. hunt
def m_hunt():
    """views/hunt_view.c :: hunt_view_draw"""
    img = screen()
    d = ImageDraw.Draw(img)

    ltext(d, 2, 10, "Find Band", PRIM)
    rtext(d, 126, 10, "14 sweeps", SEC)
    d.line([(0, 12), (128, 12)], fill=INK)

    HV_BAR_W, HV_PITCH, HV_TOP, HV_BOTTOM = 8, 9, 15, 46
    height = HV_BOTTOM - HV_TOP

    # peak-over-floor per band; index 9 (433.92) is the fob, the rest is noise
    deltas = [2, 1, 3, 4, 2, 1, 2, 3, 1, 47, 5, 3, 1, 2]
    best = 9

    d.line([(0, HV_BOTTOM), (127, HV_BOTTOM)], fill=INK)
    for i, delta in enumerate(deltas):
        x = 1 + i * HV_PITCH
        h = min(height, delta * height // 48)
        if i == best:
            if h > 0:
                d.rectangle([x, HV_BOTTOM - h, x + HV_BAR_W - 1, HV_BOTTOM], fill=INK)
            cap = max(HV_TOP - 1, HV_BOTTOM - h - 3)
            d.line([(x, cap), (x + HV_BAR_W - 1, cap)], fill=INK)
        elif h > 0:
            d.rectangle([x, HV_BOTTOM - h, x + HV_BAR_W - 1, HV_BOTTOM],
                        outline=INK, width=1)
        else:
            d.point((x + HV_BAR_W // 2, HV_BOTTOM - 1), fill=INK)

    ltext(d, 2, 54, "433.92 MHz  +47dB", SEC)
    footer(d, "OK: use this band", "Back")
    return finish(img, "screen_hunt.png")


# ----------------------------------------------------------------- 4. verdict
def m_verdict():
    """views/verdict_view.c"""
    img = screen()
    d = ImageDraw.Draw(img)

    ltext(d, 2, 10, "KeeLoq", PRIM)
    d.line([(0, 12), (128, 12)], fill=INK)

    d.rounded_rectangle([2, 15, 36, 49], radius=3, outline=INK, width=1)
    ctext(d, 19, 34, "A", BADGE)
    ctext(d, 19, 46, "Rolling", SEC)

    ltext(d, 40, 22, "Rolling code", SEC)
    ltext(d, 40, 31, "confirmed", SEC)

    d.rectangle([40, 35, 126, 43], outline=INK, width=1)
    d.rectangle([42, 37, 42 + 82 - 4, 41], fill=INK)
    for x in range(52, 126, 12):
        d.line([(x, 35), (x, 42)], fill=ORANGE, width=1)

    ltext(d, 40, 53, "3 presses . 3 codes", SEC)
    footer(d, "OK: Details", "Retest >")
    return finish(img, "screen_verdict.png")


# ----------------------------------------------------------------- 5. details
def m_details():
    """scenes/rollcall_scene_details.c - widget text scroll"""
    img = screen()
    d = ImageDraw.Draw(img)

    ltext(d, 2, 9, "KeeLoq", PRIM)
    d.line([(0, 11), (128, 11)], fill=INK)
    rows = [
        "Grade A  Rolling  HEALTHY",
        "Every press produced a",
        "DIFFERENT parcel (3",
        "presses, 3 unique codes).",
        "Presses seen",
        "1. KeeLoq 64bit  -58dBm",
        "   9A3F1C08.. (new)",
    ]
    y = 19
    for r in rows:
        ltext(d, 2, y, r, SEC)
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
    p = [m_menu(), m_capture(), m_hunt(), m_verdict(), m_details()]
    strip(p)

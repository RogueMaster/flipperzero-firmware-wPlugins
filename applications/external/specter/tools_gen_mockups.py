#!/usr/bin/env python3
"""Render Flipper-style mock screenshots (128x64, orange theme) for the README.
These mirror the on-device draw code in views/sweep_view.c and the scenes."""
from PIL import Image, ImageDraw, ImageFont
import math, os

S = 6  # scale
W, H = 128, 64
BG = (255, 130, 0)  # flipper backlight orange
FG = (10, 8, 4)  # near-black pixels
OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

# Gauge geometry (matches views/sweep_view.c)
PCX, PCY = 32, 48
R_ARC, R_OUT, R_IN, R_NDL, R_SCN = 26, 26, 22, 23, 19


def font(path, px):
    return ImageFont.truetype(path, px)


f_sec = font(MONO, 7 * S - 2)
f_pri = font(BOLD, 8 * S)
f_big = font(BOLD, 19 * S)


def canvas():
    img = Image.new("RGB", (W * S, H * S), BG)
    return img, ImageDraw.Draw(img)


def L(v):
    return int(round(v * S))


def line(d, x0, y0, x1, y1, col=FG, w=2):
    d.line([L(x0), L(y0), L(x1), L(y1)], fill=col, width=w)


def circle(d, cx, cy, r, col=FG, w=2):
    d.ellipse(
        [L(cx) - L(r), L(cy) - L(r), L(cx) + L(r), L(cy) + L(r)], outline=col, width=w
    )


def disc(d, cx, cy, r, col=FG):
    d.ellipse([L(cx) - L(r), L(cy) - L(r), L(cx) + L(r), L(cy) + L(r)], fill=col)


def box(d, x, y, w, h, col=FG):
    d.rectangle([L(x), L(y), L(x + w), L(y + h)], fill=col)


def frame(d, x, y, w, h, col=FG, lw=2):
    d.rectangle([L(x), L(y), L(x + w), L(y + h)], outline=col, width=lw)


def text(d, x, y, s, fnt=f_sec, col=FG, anchor="lm"):
    d.text((L(x), L(y)), s, font=fnt, fill=col, anchor=anchor)


def dot(d, x, y, col=FG):
    d.rectangle([L(x), L(y), L(x) + S - 1, L(y) + S - 1], fill=col)


def save(img, name):
    p = os.path.join(OUT, name)
    img.save(p)
    print("wrote", p)


def gauge_point(value, radius):
    value = max(0, min(100, value))
    a = math.pi * (1.0 - value / 100.0)
    return PCX + math.cos(a) * radius, PCY - math.sin(a) * radius


def proximity_word(s):
    return (
        "STRONG" if s >= 70 else "CLOSE" if s >= 45 else "NEAR" if s >= 20 else "FAINT"
    )


def draw_header(d, state, present):
    text(d, 2, 7, "SPECTER", f_sec)
    text(d, 116, 7, state, f_sec, anchor="rm")
    if present:
        disc(d, 123, 5, 2)
    else:
        circle(d, 123, 5, 2)
    line(d, 0, 11, 127, 11)


def draw_gauge(d, strength, peak, present, anim, scan=40):
    # arc
    px = py = None
    v = 0
    while v <= 100:
        ax, ay = gauge_point(v, R_ARC)
        if px is not None:
            line(d, px, py, ax, ay)
        px, py = ax, ay
        v += 3
    # ticks
    for i in range(11):
        vv = i * 10
        hot = i >= 8
        ox, oy = gauge_point(vv, R_OUT)
        ix, iy = gauge_point(vv, (R_IN - 3) if hot else R_IN)
        line(d, ix, iy, ox, oy)
        if hot:
            line(d, ix + 1, iy, ox + 1, oy)
    # scanner bug (idle scanning)
    if not present:
        sx, sy = gauge_point(scan, R_SCN)
        circle(d, sx, sy, 1)
    # needle
    tx, ty = gauge_point(strength, R_NDL)
    line(d, PCX, PCY, tx, ty)
    line(d, PCX - 1, PCY, tx, ty)
    disc(d, tx, ty, 1)
    # peak marker
    kx, ky = gauge_point(peak, R_OUT - 1)
    disc(d, kx, ky, 1)
    # hub
    disc(d, PCX, PCY, 3)
    if present:
        circle(d, PCX, PCY, R_OUT + 1 + (anim % 3))


def draw_readout(d, strength, peak, contacts):
    line(d, 64, 13, 64, 51)
    text(d, 68, 18, "FIELD", f_sec)
    text(d, 112, 38, str(strength), f_big, anchor="rs")
    text(d, 114, 41, "%", f_sec)
    text(d, 68, 49, f"PK{peak} C{contacts}", f_sec)


def render_sweep(name, strength, peak, contacts, present, state, history, anim=1):
    img, d = canvas()
    draw_header(d, state, present)
    draw_gauge(d, strength, peak, present, anim)
    draw_readout(d, strength, peak, contacts)
    line(d, 0, 52, 127, 52)
    if present:
        box(d, 0, 53, 128, 11)
        disc(d, 4, 58, 1, BG)
        text(d, 9, 59, "ACTIVE READER", f_sec, BG)
        text(d, 125, 59, proximity_word(strength), f_sec, BG, anchor="rm")
        frame(d, 0, 0, 127, 63, FG, lw=2)
    else:
        for k in range(62):
            idx = (len(history) - 1 - k) % len(history)
            val = history[idx]
            x = 126 - k * 2
            y = 63 - (val * 9) // 100
            if y < 63:
                line(d, x, 63, x, y, FG, w=S)
            else:
                dot(d, x, 63)
    save(img, name)


def render_menu():
    img, d = canvas()
    text(d, 4, 8, "Specter", f_pri)
    line(d, 0, 14, 127, 14)
    items = ["Sweep", "Settings", "About"]
    ROW_H = 16
    for i, it in enumerate(items):
        y = 15 + i * ROW_H
        col = FG
        if i == 0:
            box(d, 0, y, 124, ROW_H)
            col = BG
        text(d, 6, y + 8, it, f_sec, col)
    box(d, 125, 15, 3, 16)
    save(img, "screen_menu.png")


def render_settings():
    img, d = canvas()
    text(d, 4, 8, "Settings", f_pri)
    line(d, 0, 14, 127, 14)
    rows = [
        ("Sensitivity", "Medium", True),
        ("Sound", "ON", False),
        ("Vibrate", "ON", False),
        ("LED", "ON", False),
    ]
    ROW_H = 12
    for i, (k, v, sel) in enumerate(rows):
        y = 15 + i * ROW_H
        col = FG
        if sel:
            box(d, 0, y, 124, ROW_H)
            col = BG
        text(d, 4, y + 7, k, f_sec, col)
        text(d, 121, y + 7, v, f_sec, col, anchor="rm")
    box(d, 125, 15, 3, 12)
    save(img, "screen_settings.png")


CLEAR_HIST = [
    3,
    5,
    2,
    8,
    4,
    1,
    6,
    3,
    9,
    5,
    2,
    7,
    4,
    11,
    6,
    3,
    8,
    5,
    2,
    10,
    6,
    4,
    9,
    5,
    3,
    7,
    12,
    6,
    4,
    8,
    5,
    14,
    7,
    4,
    9,
    6,
    3,
    8,
    5,
    11,
    6,
    4,
    7,
    3,
    9,
    5,
    2,
    8,
    13,
    6,
    4,
    7,
    5,
    10,
    6,
    3,
    8,
    5,
    2,
    7,
    4,
    9,
]


if __name__ == "__main__":
    render_sweep("screen_clear.png", 7, 18, 0, False, "SCANNING", CLEAR_HIST, anim=2)
    render_sweep("screen_reader.png", 78, 86, 3, True, "READER", CLEAR_HIST, anim=1)
    render_menu()
    render_settings()

    names = (
        "screen_clear.png",
        "screen_reader.png",
        "screen_menu.png",
        "screen_settings.png",
    )
    imgs = [Image.open(os.path.join(OUT, n)) for n in names]
    pad = 18
    strip = Image.new(
        "RGB",
        (sum(i.width for i in imgs) + pad * (len(imgs) + 1), imgs[0].height + pad * 2),
        (12, 14, 20),
    )
    x = pad
    for im in imgs:
        strip.paste(im, (x, pad))
        x += im.width + pad
    strip.save(os.path.join(OUT, "screens.png"))
    print("wrote", os.path.join(OUT, "screens.png"))

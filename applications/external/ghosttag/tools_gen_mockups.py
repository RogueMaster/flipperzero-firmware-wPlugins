#!/usr/bin/env python3
"""Render Flipper-style mock screenshots (128x64, orange theme) for the README.
These mirror the on-device draw code in views/*.c."""
from PIL import Image, ImageDraw, ImageFont
import math, os

S = 6  # scale
W, H = 128, 64
BG = (255, 130, 0)      # flipper backlight orange
FG = (10, 8, 4)         # near-black pixels
OUT = os.path.join(os.path.dirname(__file__), "images")

MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

def font(path, px):
    return ImageFont.truetype(path, px)

f_sec = font(MONO, 7 * S - 2)      # FontSecondary ~ small
f_pri = font(BOLD, 8 * S)          # FontPrimary
f_big = font(BOLD, 22 * S)         # FontBigNumbers

def canvas():
    img = Image.new("RGB", (W * S, H * S), BG)
    return img, ImageDraw.Draw(img)

def L(v):
    return v * S

def line(d, x0, y0, x1, y1, col=FG, w=2):
    d.line([L(x0), L(y0), L(x1), L(y1)], fill=col, width=w)

def circle(d, cx, cy, r, col=FG, w=2):
    d.ellipse([L(cx) - L(r), L(cy) - L(r), L(cx) + L(r), L(cy) + L(r)], outline=col, width=w)

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


def rssi_bars(d, x, baseline, bars, col=FG):
    for i in range(4):
        h = 2 + i * 2
        bx = x + i * 4
        by = baseline - h
        if i < bars:
            box(d, bx, by, 3, h, col)
        else:
            frame(d, bx, by, 3, h, col, lw=2)


def radar_point(cx, cy, angle_deg, radius):
    a = math.radians(angle_deg)
    return cx + math.cos(a) * radius, cy + math.sin(a) * radius


# ---------------- RADAR ----------------
def render_radar():
    img, d = canvas()
    text(d, 2, 6, "GHOSTTAG", f_sec, anchor="lm")
    text(d, 113, 6, "LINK", f_sec, anchor="rm")
    disc(d, 122, 5, 2)
    line(d, 0, 12, 127, 12)

    CX, CY, R = 32, 35, 20
    circle(d, CX, CY, R)
    circle(d, CX, CY, 14)
    circle(d, CX, CY, 7)
    disc(d, CX, CY, 1)
    for off in range(-R, R + 1, 3):
        dot(d, CX + off, CY)
        dot(d, CX, CY + off)

    # sweep at 310 deg
    ex, ey = radar_point(CX, CY, 310, R)
    line(d, CX, CY, ex, ey, w=3)
    disc(d, ex, ey, 1)
    tx, ty = radar_point(CX, CY, 298, R - 4)
    line(d, CX, CY, tx, ty)

    # blips: (angle, radius, following)
    blips = [(40, 16, False), (120, 11, False), (210, 18, False), (95, 8, True)]
    for ang, rad, foll in blips:
        bx, by = radar_point(CX, CY, ang, rad)
        if foll:
            disc(d, bx, by, 2)
            circle(d, bx, by, 4)
        else:
            disc(d, bx, by, 1)

    text(d, 60, 22, "TRACKERS", f_sec, anchor="lm")
    text(d, 62, 42, "4", f_big, anchor="lm")

    box(d, 0, 54, 128, 10)
    text(d, 3, 60, "! 1 FOLLOWING", f_sec, BG, anchor="lm")
    text(d, 125, 60, "OK", f_sec, BG, anchor="rm")
    save(img, "screen_radar.png")


# ---------------- LIST ----------------
def render_list():
    img, d = canvas()
    text(d, 2, 6, "Detections", f_pri, anchor="lm")
    text(d, 125, 6, "4", f_sec, anchor="rm")
    line(d, 0, 12, 127, 12)

    rows = [
        ("AirTag 4F:A2", 4, True),
        ("Tile 9C:01", 3, False),
        ("SmartTag 7E:33", 2, False),
        ("Unknown B1:0D", 1, False),
    ]
    ROW_H = 13
    for i, (label, bars, foll) in enumerate(rows):
        y = 13 + i * ROW_H
        baseline = y + 6
        sel = (i == 0)
        col = FG
        if sel:
            box(d, 0, y, 122, ROW_H)
            col = BG
        # icon placeholder (small ghost/tag glyph as filled rounded box)
        if "AirTag" in label:
            disc(d, 7, y + 5, 4, col)
            disc(d, 5, y + 4, 1, BG if sel else BG)
        else:
            frame(d, 3, y + 2, 8, 9, col, lw=2)
        text(d, 15, baseline, label, f_sec, col, anchor="lm")
        rssi_bars(d, 90, baseline + 4, bars, col)
        if foll:
            # warning marker
            line(d, 112, y + 2, 116, y + 10, col)
            line(d, 116, y + 2, 112, y + 10, col)
            line(d, 112, y + 10, 116, y + 10, col)
            text(d, 114, baseline, "!", f_sec, col, anchor="mm")
        if sel:
            pass
    # scrollbar
    box(d, 125, 13, 2, 12)
    save(img, "screen_list.png")


# ---------------- ALERT ----------------
def render_alert():
    img, d = canvas()
    box(d, 0, 0, 128, 14)
    text(d, 64, 6, "! TRACKER ALERT !", f_pri, BG, anchor="mm")

    # big warning triangle
    tx, ty, lx, ly, rx, ry = 24, 16, 8, 50, 40, 50
    line(d, tx, ty, lx, ly, w=3)
    line(d, tx, ty, rx, ry, w=3)
    line(d, lx, ly, rx, ry, w=3)
    box(d, 23, 26, 3, 13)
    box(d, 23, 44, 3, 3)

    text(d, 48, 24, "AirTag", f_pri, anchor="lm")
    text(d, 48, 37, "with you 03:12", f_sec, anchor="lm")
    text(d, 48, 49, "is following you", f_sec, anchor="lm")

    line(d, 0, 53, 127, 53)
    text(d, 2, 60, "OK: details", f_sec, anchor="lm")
    text(d, 125, 60, "Back: dismiss", f_sec, anchor="rm")
    save(img, "screen_alert.png")


if __name__ == "__main__":
    render_radar()
    render_list()
    render_alert()

    # combined strip for the README
    imgs = [Image.open(os.path.join(OUT, n)) for n in
            ("screen_radar.png", "screen_list.png", "screen_alert.png")]
    pad = 18
    strip = Image.new("RGB", (sum(i.width for i in imgs) + pad * (len(imgs) + 1),
                              imgs[0].height + pad * 2), (12, 16, 20))
    x = pad
    for im in imgs:
        strip.paste(im, (x, pad))
        x += im.width + pad
    strip.save(os.path.join(OUT, "screens.png"))
    print("wrote", os.path.join(OUT, "screens.png"))

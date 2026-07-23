#!/usr/bin/env python3
"""Render Flipper-style mock screenshots (128x64, orange theme) for the README.
These mirror the on-device draw code in views/*.c."""
from PIL import Image, ImageDraw, ImageFont
import math, os

S = 6  # scale
W, H = 128, 64
BG = (255, 130, 0)  # flipper backlight orange
FG = (10, 8, 4)  # near-black pixels
OUT = os.path.join(os.path.dirname(__file__), "images")

MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

# Eye geometry (matches views/monitor_view.c)
CX, CY, RX, LIDH, IRISR = 34, 33, 31, 18, 16


def font(path, px):
    return ImageFont.truetype(path, px)


f_sec = font(MONO, 7 * S - 2)
f_pri = font(BOLD, 8 * S)
f_big = font(BOLD, 20 * S)


def canvas():
    img = Image.new("RGB", (W * S, H * S), BG)
    return img, ImageDraw.Draw(img)


def L(v):
    return int(v * S)


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


def radar_point(angle_deg, radius):
    a = math.radians(angle_deg)
    return CX + math.cos(a) * radius, CY + math.sin(a) * radius


def eye_lid(d, sign, yoff=0, col=FG, w=2):
    pts = []
    for x in range(-RX, RX + 1):
        t = x / RX
        off = int(LIDH * (1 - t * t))
        pts.append((L(CX + x), L(CY + sign * off + sign * yoff)))
    d.line(pts, fill=col, width=w, joint="curve")


def rssi_bars(d, x, baseline, bars, col=FG):
    for i in range(4):
        h = 2 + i * 2
        bx = x + i * 4
        by = baseline - h
        if i < bars:
            box(d, bx, by, 3, h, col)
        else:
            frame(d, bx, by, 3, h, col, lw=2)


def draw_header(d, link="LIVE", connected=True):
    text(d, 2, 7, "ARGUS", f_sec)
    text(d, 113, 7, link, f_sec, anchor="rm")
    if connected:
        disc(d, 122, 5, 2)
    else:
        circle(d, 122, 5, 2)
    line(d, 0, 12, 127, 12)


def draw_eye(d, sweep_deg, blips, attack=False):
    eye_lid(d, -1)
    eye_lid(d, +1)
    if attack:
        eye_lid(d, -1, 2)
        eye_lid(d, +1, 2)
    circle(d, CX, CY, IRISR)
    circle(d, CX, CY, IRISR - 6)
    for off in range(-(IRISR - 2), IRISR - 1, 3):
        dot(d, CX + off, CY)
        dot(d, CX, CY + off)
    ex, ey = radar_point(sweep_deg, IRISR - 1)
    line(d, CX, CY, ex, ey)
    disc(d, ex, ey, 1)
    for ang, rad, clone in blips:
        bx, by = radar_point(ang, rad)
        if clone:
            disc(d, bx, by, 2)
            circle(d, bx, by, 4)
        else:
            disc(d, bx, by, 1)
    if attack:
        box(d, CX - 1, CY - (IRISR - 5), 3, 2 * (IRISR - 5))
    else:
        disc(d, CX, CY, 2)


def draw_stats(d, deauths, ap, tw):
    line(d, 67, 13, 67, 52)
    text(d, 71, 18, "DEAUTHS", f_sec)
    text(d, 72, 36, str(deauths), f_big, anchor="lm")
    text(d, 71, 49, f"AP {ap}  TW {tw}", f_sec)


# ---------------- WATCH (calm) ----------------
def render_watch():
    img, d = canvas()
    draw_header(d, "LIVE", True)
    blips = [(40, 10, False), (150, 13, False), (250, 8, False), (95, 6, False)]
    draw_eye(d, 310, blips)
    draw_stats(d, 0, 4, 0)
    text(d, 2, 60, "Guarding HomeWiFi", f_sec)
    text(d, 125, 60, "OK:log", f_sec, anchor="rm")
    save(img, "screen_watch.png")


# ---------------- WATCH (under attack) ----------------
def render_attack():
    img, d = canvas()
    draw_header(d, "LIVE", True)
    blips = [(40, 10, False), (150, 13, True), (250, 8, False), (95, 6, False)]
    draw_eye(d, 70, blips, attack=True)
    draw_stats(d, 47, 5, 1)
    box(d, 0, 54, 128, 10)
    text(d, 3, 60, "! DEAUTH ATTACK x9", f_sec, BG)
    frame(d, 0, 0, 127, 63, FG, lw=2)
    save(img, "screen_attack.png")


# ---------------- EVIL TWINS list ----------------
def render_twins():
    img, d = canvas()
    text(d, 2, 7, "Evil Twins", f_pri)
    text(d, 125, 7, "3", f_sec, anchor="rm")
    line(d, 0, 12, 127, 12)
    rows = [
        ("HomeWiFi", "A2:1F:0C", "c6 WPA2 -42dBm", False, True),
        ("HomeWiFi", "9C:55:7E", "c11 Open -55dBm", True, False),
        ("HomeWiFi", "7E:33:B1", "c1 WPA2 -71dBm", True, False),
    ]
    ROW_H = 16
    for i, (ssid, bssid, meta, clone, sel) in enumerate(rows):
        y = 13 + i * ROW_H
        col = FG
        if sel:
            box(d, 0, y, 124, ROW_H)
            col = BG
        if clone:
            line(d, 3, y + 2, 3, y + 8, col)
            dot(d, 3, y + 10, col)
            line(d, 2, y + 2, 4, y + 2, col)
        else:
            disc(d, 3, y + 6, 1, col)
        text(d, 9, y + 6, ssid, f_sec, col)
        text(d, 121, y + 6, bssid, f_sec, col, anchor="rm")
        text(d, 9, y + 13, meta, f_sec, col)
        if clone:
            text(d, 121, y + 13, "TWIN", f_sec, col, anchor="rm")
    box(d, 125, 13, 3, 16)
    save(img, "screen_twins.png")


# ---------------- THREAT LOG ----------------
def render_log():
    img, d = canvas()
    text(d, 2, 7, "Threat Log", f_pri)
    text(d, 125, 7, "12", f_sec, anchor="rm")
    line(d, 0, 12, 127, 12)
    rows = [
        ("EVIL TWIN", "TWIN HomeWiFi", "3s", True),
        ("DEAUTH", "DEAUTH c6  -42dBm", "5s", False),
        ("DEAUTH", "DEAUTH c6  -45dBm", "5s", False),
        ("DISASS", "DISASSOC c6 -50dBm", "9s", False),
    ]
    ROW_H = 13
    for i, (_k, body, age, sel) in enumerate(rows):
        y = 13 + i * ROW_H
        base = y + 10
        col = FG
        if sel:
            box(d, 0, y, 124, ROW_H)
            col = BG
        text(d, 3, base, body, f_sec, col)
        text(d, 121, base, age, f_sec, col, anchor="rm")
    box(d, 125, 13, 3, 14)
    save(img, "screen_log.png")


if __name__ == "__main__":
    render_watch()
    render_attack()
    render_twins()
    render_log()

    # combined strip for the README
    names = (
        "screen_watch.png",
        "screen_attack.png",
        "screen_twins.png",
        "screen_log.png",
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

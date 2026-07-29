#!/usr/bin/env python3
"""Render Flipper-style mock screenshots (128x64, orange theme) for the README.
These mirror the on-device draw code in views/sweep_view.c, views/probe_view.c,
views/splash_view.c and the scenes, so the README shows what the app draws."""
from PIL import Image, ImageDraw, ImageFont
import math
import os

S = 6  # scale
W, H = 128, 64
BG = (255, 130, 0)  # flipper backlight orange
FG = (10, 8, 4)  # near-black pixels
OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

# sweep-view eye geometry — must match the #defines in views/sweep_view.c
EYE_CX, EYE_CY, EYE_ROUT, EYE_IRIS = 21, 27, 15, 9


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


def tri_up(d, x, y, base, height, col=FG):
    d.polygon([(L(x), L(y)), (L(x - base / 2), L(y + height)), (L(x + base / 2), L(y + height))], fill=col)


def tri_down(d, x, y, base, height, col=FG):
    d.polygon([(L(x), L(y)), (L(x - base / 2), L(y - height)), (L(x + base / 2), L(y - height))], fill=col)


def save(img, name):
    p = os.path.join(OUT, name)
    img.save(p)
    print("wrote", p)


def proximity_word(s):
    return "STRONG" if s >= 70 else "CLOSE" if s >= 45 else "NEAR" if s >= 20 else "FAINT"


def draw_header(d, mode_word, present):
    text(d, 2, 7, "NYX", f_sec)
    text(d, 116, 7, mode_word, f_sec, anchor="rm")
    if present:
        disc(d, 123, 5, 2)
    else:
        circle(d, 123, 5, 2)
    line(d, 0, 11, 127, 11)


def draw_trend_at(d, cx, cy, trend):
    if trend > 0:
        tri_up(d, cx, cy - 4, 12, 9)
    elif trend < 0:
        tri_down(d, cx, cy + 4, 12, 9)
    else:
        box(d, cx - 6, cy - 1, 12, 3)


def ring_point(radius, deg):
    a = math.radians(deg - 90)
    return EYE_CX + math.cos(a) * radius, EYE_CY + math.sin(a) * radius


def draw_eye(d, level, peak, present, anim):
    circle(d, EYE_CX, EYE_CY, EYE_ROUT)
    # proximity arc, clockwise from 12 o'clock
    span = (level * 360) // 100
    for deg in range(0, span, 5):
        for rr in (EYE_ROUT - 1, EYE_ROUT - 2):
            x, y = ring_point(rr, deg)
            dot(d, x, y)
    # peak tick
    if peak > 0:
        xi, yi = ring_point(EYE_ROUT - 4, (peak * 360) // 100)
        xo, yo = ring_point(EYE_ROUT + 1, (peak * 360) // 100)
        line(d, xi, yi, xo, yo)
    # iris + dilating pupil
    circle(d, EYE_CX, EYE_CY, EYE_IRIS)
    pupil = 1 + (level * 7) // 100
    disc(d, EYE_CX, EYE_CY, pupil)
    # lock-on glare
    if present:
        for i in range(6):
            deg = i * 60 + anim * 6
            x1, y1 = ring_point(EYE_IRIS + 2, deg)
            x2, y2 = ring_point(EYE_IRIS + 4, deg)
            line(d, x1, y1, x2, y2)


def render_sweep(name, level, peak, hits, trend, kind, present, mode_word, hint=None, anim=0):
    img, d = canvas()
    draw_header(d, mode_word, present)

    # eye gauge (left)
    draw_eye(d, level, peak, present, anim)
    text(d, EYE_CX, 50, f"{level}%", f_sec, anchor="ms")

    # readout (right)
    line(d, 41, 13, 41, 51)
    text(d, 45, 21, kind, f_pri)
    state = proximity_word(level) if present else ("SCANNING" if level or hint else "IDLE")
    text(d, 45, 35, state, f_sec)
    text(d, 45, 48, f"PK{peak}  HIT{hits}", f_sec)
    draw_trend_at(d, 118, 30, trend)

    # status strip
    line(d, 0, 52, 127, 52)
    if present:
        box(d, 0, 53, 128, 11)
        disc(d, 4, 58, 1, BG)
        text(d, 9, 59, "IR EMITTER", f_sec, BG)
        text(d, 125, 59, proximity_word(level), f_sec, BG, anchor="rm")
        frame(d, 0, 0, 127, 63, FG, lw=2)
    else:
        text(d, 2, 59, hint or "Pan slowly across walls", f_sec)
    save(img, name)


def render_splash(name):
    img, d = canvas()
    cx, cy, hw, hh = 64, 27, 30, 14
    # IR wave-rings washing out of the pupil
    for r in (10, 22, 34):
        circle(d, cx, cy, r, FG, w=2)
    # almond eye (fully open frame)
    for sign in (-1, 1):
        pts = []
        for i in range(0, 49):
            t = i / 48.0
            x = cx - hw + t * 2 * hw
            y = cy + sign * math.sin(t * math.pi) * hh
            pts.append((L(x), L(y)))
        d.line(pts, fill=FG, width=2)
    circle(d, cx, cy, 7)
    disc(d, cx, cy, 3)
    text(d, 64, 50, "N Y X", f_pri, anchor="ms")
    text(d, 64, 61, "IR EMITTER SWEEP", f_sec, anchor="ms")
    save(img, name)


def render_nulling(name):
    img, d = canvas()
    draw_header(d, "PROBE", False)
    text(d, 64, 26, "Nulling ambient", f_pri, anchor="mm")
    text(d, 64, 40, "Hold still, aim at the room", f_sec, anchor="mm")
    for i in range(3):
        x = 56 + i * 8
        if i <= 1:
            disc(d, x, 52, 2)
        else:
            circle(d, x, 52, 2)
    save(img, name)


def render_menu():
    img, d = canvas()
    text(d, 4, 8, "Nyx", f_pri)
    line(d, 0, 14, 127, 14)
    items = ["Sweep", "Probe Setup", "Settings", "About"]
    ROW_H = 12
    for i, it in enumerate(items):
        y = 15 + i * ROW_H
        col = FG
        if i == 0:
            box(d, 0, y, 124, ROW_H)
            col = BG
        text(d, 6, y + 7, it, f_sec, col)
    box(d, 125, 15, 3, 12)
    save(img, "screen_menu.png")


def render_probe_wiring():
    img, d = canvas()
    text(d, 2, 7, "PROBE WIRING", f_sec)
    text(d, 126, 7, "1/2", f_sec, anchor="rm")
    line(d, 0, 11, 127, 11)

    # 3V3 rail
    line(d, 14, 17, 26, 17)
    text(d, 29, 18, "3V3", f_sec)
    # phototransistor
    line(d, 20, 17, 20, 21)
    circle(d, 20, 27, 6)
    line(d, 16, 31, 24, 23)
    # light arrows
    for ay in (24, 32):
        line(d, 5, ay - 7, 12, ay)
        line(d, 12, ay, 8, ay)
        line(d, 12, ay, 12, ay - 4)
    line(d, 20, 33, 20, 42)
    disc(d, 20, 38, 1)
    line(d, 20, 38, 42, 38)
    text(d, 44, 39, "ADC", f_sec)
    frame(d, 17, 42, 7, 11)
    text(d, 27, 48, "10k", f_sec)
    line(d, 20, 53, 20, 57)
    line(d, 14, 57, 26, 57)
    line(d, 16, 59, 24, 59)
    line(d, 18, 61, 22, 61)

    line(d, 60, 12, 60, 63)
    text(d, 64, 18, "PHOTO-TR", f_sec)
    text(d, 64, 29, "C  3V3   p9", f_sec)
    text(d, 64, 39, "E  PC0   p16", f_sec)
    text(d, 64, 49, "E  10k to", f_sec)
    text(d, 64, 59, "   GND  p18", f_sec)
    save(img, "screen_probe_wiring.png")


def render_probe_check():
    img, d = canvas()
    text(d, 2, 7, "PROBE CHECK", f_sec)
    text(d, 126, 7, "2/2", f_sec, anchor="rm")
    line(d, 0, 11, 127, 11)

    box(d, 6, 14, 116, 13)
    text(d, 64, 21, "PROBE DETECTED", f_pri, BG, anchor="mm")

    text(d, 74, 44, "412", f_big, anchor="rs")
    text(d, 77, 41, "mV", f_sec)
    text(d, 126, 41, "pk 655", f_sec, anchor="rm")

    frame(d, 2, 47, 124, 7)
    w = (412 * 122) // 2500
    box(d, 3, 48, w, 5)
    pk = (655 * 122) // 2500
    line(d, 3 + pk, 46, 3 + pk, 54)
    text(d, 2, 59, "Aim a TV remote, press a key", f_sec)
    save(img, "screen_probe_check.png")


def render_settings():
    img, d = canvas()
    text(d, 4, 8, "Settings", f_pri)
    line(d, 0, 14, 127, 14)
    rows = [("Mode", "Auto", True), ("Sensitivity", "Medium", False),
            ("Probe pin", "PC0", False), ("Sound", "ON", False)]
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


if __name__ == "__main__":
    render_splash("screen_splash.png")
    render_sweep("screen_clear.png", 4, 9, 0, 0, "--", False, "ONBOARD",
                 hint="Onboard: pulsed IR only")
    render_sweep("screen_emitter.png", 84, 90, 3, 1, "STEADY", True, "PROBE", anim=2)
    render_nulling("screen_nulling.png")
    render_menu()
    render_probe_wiring()
    render_probe_check()
    render_settings()

    names = ("screen_splash.png", "screen_emitter.png", "screen_probe_wiring.png",
             "screen_settings.png")
    imgs = [Image.open(os.path.join(OUT, n)) for n in names]
    pad = 18
    strip = Image.new(
        "RGB",
        (sum(i.width for i in imgs) + pad * (len(imgs) + 1), imgs[0].height + pad * 2),
        (14, 11, 22),
    )
    x = pad
    for im in imgs:
        strip.paste(im, (x, pad))
        x += im.width + pad
    strip.save(os.path.join(OUT, "screens.png"))
    print("wrote", os.path.join(OUT, "screens.png"))

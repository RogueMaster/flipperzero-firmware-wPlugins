#!/usr/bin/env python3
"""Render Flipper-style mock screenshots (128x64, orange theme) for the README.
These mirror the on-device draw code in views/sweep_view.c, views/probe_view.c
and the scenes, so the README shows what the app actually draws."""
from PIL import Image, ImageDraw, ImageFont
import os

S = 6  # scale
W, H = 128, 64
BG = (255, 130, 0)  # flipper backlight orange
FG = (10, 8, 4)  # near-black pixels
OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

TRACE_BASE_Y = 51
TRACE_H = 13
NYX_TRACE_LEN = 64


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


def draw_trend(d, trend):
    if trend > 0:
        # base at y=30, points up (height 9) => apex at y=21
        tri_up(d, 58, 21, 11, 9)
    elif trend < 0:
        tri_down(d, 58, 30, 11, 9)
    else:
        box(d, 53, 24, 11, 3)


def draw_trace(d, trace, peak):
    for k in range(NYX_TRACE_LEN):
        idx = (len(trace) - 1 - k) % len(trace)
        val = trace[idx]
        h = (val * TRACE_H) // 100
        x = 126 - k * 2
        if h > 0:
            line(d, x, TRACE_BASE_Y, x, TRACE_BASE_Y - h, FG, w=S)
        else:
            dot(d, x, TRACE_BASE_Y)
    if peak > 0:
        py = TRACE_BASE_Y - (peak * TRACE_H) // 100
        x = 0
        while x < 128:
            dot(d, x, py)
            x += 4


def render_sweep(name, level, peak, hits, trend, kind, present, mode_word, trace, hint=None):
    img, d = canvas()
    draw_header(d, mode_word, present)

    # readout
    text(d, 42, 33, str(level), f_big, anchor="rs")
    text(d, 44, 30, "%", f_sec)
    draw_trend(d, trend)
    line(d, 68, 13, 68, 34)
    text(d, 72, 20, kind, f_sec)
    text(d, 72, 31, f"PK{peak} H{hits}", f_sec)

    # trace
    line(d, 0, 36, 127, 36)
    draw_trace(d, trace, peak)

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


# recent-level ring buffers for the trace
CLEAR = [3, 5, 2, 8, 4, 1, 6, 3, 9, 5, 2, 7, 4, 11, 6, 3, 8, 5, 2, 10, 6, 4, 9,
         5, 3, 7, 12, 6, 4, 8, 5, 14, 7, 4, 9, 6, 3, 8, 5, 11, 6, 4, 7, 3, 9, 5,
         2, 8, 13, 6, 4, 7, 5, 10, 6, 3, 8, 5, 2, 7, 4, 9]
HOT = [4, 6, 5, 9, 8, 12, 15, 18, 22, 20, 26, 30, 28, 35, 40, 38, 44, 50, 48, 55,
       60, 58, 64, 68, 66, 72, 76, 74, 80, 78, 82, 85, 83, 86, 88, 84, 87, 85, 82,
       86, 88, 90, 87, 85, 88, 84, 86, 89, 85, 83, 87, 84, 86, 88, 85, 82, 86, 88,
       84, 87, 85, 83, 86, 88]


if __name__ == "__main__":
    render_sweep("screen_clear.png", 4, 9, 0, 0, "--", False, "ONBOARD", CLEAR,
                 hint="Onboard: pulsed IR only")
    render_sweep("screen_emitter.png", 84, 90, 3, 1, "STEADY", True, "PROBE", HOT)
    render_nulling("screen_nulling.png")
    render_menu()
    render_probe_wiring()
    render_probe_check()
    render_settings()

    names = ("screen_clear.png", "screen_emitter.png", "screen_probe_wiring.png",
             "screen_probe_check.png")
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

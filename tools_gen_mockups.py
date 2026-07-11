#!/usr/bin/env python3
"""Render Flipper-style mock screenshots (128x64, orange theme) for the README.
These mirror the on-device draw code in views/spectrum_view.c,
views/console_view.c and the submenus."""
from PIL import Image, ImageDraw, ImageFont
import os, math

S = 6  # scale
W, H = 128, 64
BG = (255, 130, 0)  # flipper backlight orange
FG = (10, 8, 4)  # near-black pixels
OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

# 10x10 app mark (matches icons/trident_10px.png)
TRIDENT = [
    ".#..#..#..",
    ".#..#..#..",
    ".#..#..#..",
    ".#######..",
    "....#.....",
    "....#.....",
    "....#.....",
    "...###....",
    "....#.....",
    "..........",
]
SKULL = [
    "..######..",
    ".########.",
    "##..##..##",
    "##..##..##",
    ".########.",
    ".###..###.",
    ".########.",
    ".##.##.##.",
    "..######..",
    "..........",
]


def font(path, px):
    try:
        return ImageFont.truetype(path, px)
    except OSError:
        return ImageFont.truetype(BOLD, px)


f_sec = font(MONO, 7 * S - 4)
f_key = font(MONO, 6 * S - 2)
f_pri = font(BOLD, 8 * S)


def canvas():
    img = Image.new("RGB", (W * S, H * S), BG)
    return img, ImageDraw.Draw(img)


def L(v):
    return int(v * S)


def line(d, x0, y0, x1, y1, col=FG, w=2):
    d.line([L(x0), L(y0), L(x1), L(y1)], fill=col, width=w)


def circle(d, cx, cy, r, col=FG, w=2):
    d.ellipse([L(cx) - L(r), L(cy) - L(r), L(cx) + L(r), L(cy) + L(r)], outline=col, width=w)


def disc(d, cx, cy, r, col=FG):
    d.ellipse([L(cx) - L(r), L(cy) - L(r), L(cx) + L(r), L(cy) + L(r)], fill=col)


def rbox(d, x, y, w, h, col=FG, rad=3):
    d.rounded_rectangle([L(x), L(y), L(x + w), L(y + h)], radius=L(rad), fill=col)


def text(d, x, y, s, fnt=f_sec, col=FG, anchor="lm"):
    d.text((L(x), L(y)), s, font=fnt, fill=col, anchor=anchor)


def glyph(d, rows, ox, oy, col=FG):
    for gy, row in enumerate(rows):
        for gx, ch in enumerate(row):
            if ch == "#":
                d.rectangle(
                    [L(ox + gx), L(oy + gy), L(ox + gx) + S - 1, L(oy + gy) + S - 1], fill=col
                )


def save(img, name):
    p = os.path.join(OUT, name)
    img.save(p)
    print("wrote", p)


# ---------- submenu ----------
def submenu(header, items, selected):
    img, d = canvas()
    text(d, 3, 6, header, fnt=f_pri, anchor="lm")
    line(d, 0, 12, 127, 12)
    y = 15
    for i, it in enumerate(items):
        if i == selected:
            rbox(d, 1, y, 126, 11, col=FG, rad=3)
            text(d, 5, y + 6, it, fnt=f_sec, col=BG, anchor="lm")
        else:
            text(d, 5, y + 6, it, fnt=f_sec, col=FG, anchor="lm")
        y += 12
    return img


def varlist(header, rows, selected):
    img, d = canvas()
    text(d, 3, 6, header, fnt=f_pri, anchor="lm")
    line(d, 0, 12, 127, 12)
    y = 15
    for i, (name, val) in enumerate(rows):
        if i == selected:
            rbox(d, 1, y, 126, 11, col=FG, rad=3)
            text(d, 5, y + 6, name, fnt=f_sec, col=BG, anchor="lm")
            text(d, 124, y + 6, "< " + val + " >", fnt=f_sec, col=BG, anchor="rm")
        else:
            text(d, 5, y + 6, name, fnt=f_sec, col=FG, anchor="lm")
            text(d, 124, y + 6, val, fnt=f_sec, col=FG, anchor="rm")
        y += 12
    return img


# ---------- spectrum analyzer (mirrors views/spectrum_view.c) ----------
def spectrum(title, levels, lo, hi, peak_label, running=True):
    img, d = canvas()
    glyph(d, TRIDENT, 0, 1)
    text(d, 13, 5, title, fnt=f_sec, anchor="lm")
    text(d, 110, 5, "SCAN" if running else "IDLE", fnt=f_sec, anchor="rm")
    if running:
        disc(d, 124, 5, 2)
    else:
        circle(d, 124, 5, 2)
    line(d, 0, 12, 127, 12)

    base, top = 51, 15
    gh = base - top
    line(d, 0, base + 1, 127, base + 1)
    n = len(levels)
    peak_i, peak_v = -1, -1
    for i, lv in enumerate(levels):
        x = int(i * 127 / (n - 1))
        h = int(lv * gh / 100)
        if h > 0:
            line(d, x, base, x, base - h, w=2)
        if lv > peak_v:
            peak_v, peak_i = lv, i
    if peak_i >= 0:
        px = int(peak_i * 127 / (n - 1))
        line(d, px, top - 1, px - 2, top - 3, w=2)
        line(d, px, top - 1, px + 2, top - 3, w=2)

    line(d, 0, 52, 127, 52)
    text(d, 0, 60, lo, fnt=f_sec, anchor="lm")
    text(d, 64, 60, peak_label, fnt=f_sec, anchor="mm")
    text(d, 127, 60, hi, fnt=f_sec, anchor="rm")
    return img


# ---------- console (ESP32) ----------
def console(title, lines, chan="13/14", live=True):
    img, d = canvas()
    glyph(d, TRIDENT, 0, 1)
    text(d, 13, 5, title, fnt=f_sec, anchor="lm")
    text(d, 110, 5, "LIVE" if live else "IDLE", fnt=f_sec, anchor="rm")
    if live:
        disc(d, 124, 5, 2)
    else:
        circle(d, 124, 5, 2)
    line(d, 0, 12, 127, 12)
    y = 17
    for ln in lines[:5]:
        text(d, 1, y, ln, fnt=f_key, anchor="lm")
        y += 8
    line(d, 0, 54, 127, 54)
    text(d, 2, 60, "OK:cmd", fnt=f_sec, anchor="lm")
    text(d, 126, 60, "UART " + chan, fnt=f_sec, anchor="rm")
    return img


# ---------- confirm (attack gate) ----------
def confirm(op):
    img, d = canvas()
    glyph(d, SKULL, 2, 2)
    text(d, 16, 8, "Start: " + op, fnt=f_pri, anchor="lm")
    body = ["This transmits and can", "disrupt nearby devices.", "Test only what you own."]
    y = 24
    for b in body:
        text(d, 2, y, b, fnt=f_key, anchor="lm")
        y += 9
    rbox(d, 42, 52, 44, 11, col=FG, rad=3)
    text(d, 64, 58, "Start", fnt=f_sec, col=BG, anchor="mm")
    return img


def synth(n, peaks, floor=6, seed=1):
    """Fake but plausible spectrum: low noise floor with a few peaks."""
    import random
    random.seed(seed)
    out = []
    for i in range(n):
        v = floor + random.randint(0, 8)
        for (c, amp, wdt) in peaks:
            v += int(amp * math.exp(-((i - c) ** 2) / (2 * wdt * wdt)))
        out.append(min(100, v))
    return out


def main():
    save(
        submenu(
            "Trident",
            ["ESP32  Wi-Fi / BT", "NRF24  2.4 GHz", "CC1101  Sub-GHz", "ESP32 Console"],
            1,
        ),
        "screen_home.png",
    )
    save(
        spectrum(
            "NRF24 2.4GHz",
            synth(126, [(11, 78, 4), (52, 92, 3), (76, 60, 5)], seed=4),
            "2400", "2525", "Ch52 2452MHz",
        ),
        "screen_nrf24.png",
    )
    save(
        spectrum(
            "CC1101 int",
            synth(60, [(24, 88, 2), (41, 55, 3)], seed=9),
            "387", "464", "433.9M -47dBm",
        ),
        "screen_subghz.png",
    )
    save(
        console(
            "Scan APs",
            [
                "Scanning 2.4 + 5 GHz",
                "0| HomeNet     6  -47",
                "1| Office_5G  36  -60",
                "2| cafe-wifi  11  -72",
                "3| <hidden>  149  -80",
            ],
        ),
        "screen_console.png",
    )
    save(confirm("Deauth Flood"), "screen_confirm.png")
    save(
        varlist(
            "Settings",
            [
                ("ESP32 UART pins", "13/14"),
                ("CC1101 radio", "Internal"),
                ("Sub-GHz band", "387-464"),
                ("Confirm attacks", "ON"),
            ],
            1,
        ),
        "screen_settings.png",
    )

    heroes = ["screen_home.png", "screen_nrf24.png", "screen_subghz.png", "screen_console.png"]
    imgs = [Image.open(os.path.join(OUT, n)) for n in heroes]
    pad = 12
    total_w = sum(i.width for i in imgs) + pad * (len(imgs) + 1)
    strip = Image.new("RGB", (total_w, imgs[0].height + pad * 2), (18, 18, 24))
    x = pad
    for im in imgs:
        strip.paste(im, (x, pad))
        x += im.width + pad
    save(strip, "screens.png")


if __name__ == "__main__":
    main()

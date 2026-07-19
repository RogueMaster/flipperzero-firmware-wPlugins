#!/usr/bin/env python3
"""Render Flipper-style mock screenshots (128x64, orange backlight) for the README.

These mirror the on-device draw code in views/meter_view.c line for line, so a
layout collision shows up here before it ships. Text is positioned by BASELINE
(PIL anchor "ls"/"rs"/"ms") because canvas_draw_str takes y as the baseline.
"""
from PIL import Image, ImageDraw, ImageFont
import os

S = 6  # upscale factor
W, H = 128, 64
BG = (255, 130, 0)  # flipper backlight orange
FG = (10, 8, 4)  # near-black pixels
OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

f_sec = ImageFont.truetype(MONO, 7 * S - 2)  # FontSecondary
f_pri = ImageFont.truetype(BOLD, 8 * S)  # FontPrimary
f_big = ImageFont.truetype(BOLD, 19 * S)  # FontBigNumbers


def canvas():
    img = Image.new("RGB", (W * S, H * S), BG)
    return img, ImageDraw.Draw(img)


def L(v):
    return int(round(v * S))


def line(d, x0, y0, x1, y1, col=FG, w=2):
    d.line([L(x0), L(y0), L(x1), L(y1)], fill=col, width=w)


def box(d, x, y, w, h, col=FG):
    """canvas_draw_box: filled, inclusive of (x,y)..(x+w-1, y+h-1)."""
    d.rectangle([L(x), L(y), L(x + w) - 1, L(y + h) - 1], fill=col)


def frame(d, x, y, w, h, col=FG, lw=2):
    d.rectangle([L(x), L(y), L(x + w) - 1, L(y + h) - 1], outline=col, width=lw)


def rframe(d, x, y, w, h, r, col=FG, lw=2):
    d.rounded_rectangle(
        [L(x), L(y), L(x + w) - 1, L(y + h) - 1], radius=L(r), outline=col, width=lw
    )


def rbox(d, x, y, w, h, r, col=FG):
    d.rounded_rectangle([L(x), L(y), L(x + w) - 1, L(y + h) - 1], radius=L(r), fill=col)


def text(d, x, y, s, fnt=f_sec, col=FG, anchor="ls"):
    """y is the BASELINE, matching canvas_draw_str."""
    d.text((L(x), L(y)), s, font=fnt, fill=col, anchor=anchor)


def tw(s, fnt=f_sec):
    """String width in Flipper pixels (mirrors canvas_string_width)."""
    return fnt.getlength(s) / S


def save(img, name):
    p = os.path.join(OUT, name)
    img.save(p)
    print("wrote", p)


# ---------------- mirrored draw helpers ----------------


def draw_bar(d, x, y, w, h, fill_pct, peak_pct=-1):
    frame(d, x, y, w, h)
    inner = w - 4
    fw = (inner * min(fill_pct, 100)) // 100
    if fw > 0:
        box(d, x + 2, y + 2, fw, h - 4)
    if peak_pct >= 0:
        px = x + 2 + (inner * min(peak_pct, 100)) // 100
        line(d, px, y - 1, px, y + h)


def draw_pips(d, x, y, filled, col=FG):
    for i in range(5):
        px = x + i * 5
        if i < filled:
            box(d, px, y, 3, 3, col)
        else:
            frame(d, px, y, 3, 3, col, lw=2)


def draw_big_value(d, x_right, baseline, value, unit):
    """Right-aligned big number with a hand-drawn minus and unit above."""
    mag = abs(value)
    s = str(mag)
    w = tw(s, f_big)
    x = x_right - w
    text(d, x_right, baseline, s, f_big, anchor="rs")
    if value < 0:
        box(d, x - 8, baseline - 8, 5, 2)
    if unit:
        text(d, x_right, baseline - 17, unit, f_sec, anchor="rs")


def draw_header(d, band):
    text(d, 2, 9, "FARADAY", f_sec)
    text(d, 126, 9, band, f_sec, anchor="rs")
    line(d, 0, 11, 127, 11)


# ---------------- faces ----------------


def render_capture(name, *, is_nfc, phase, level, peak, live, signal_ok, base=None):
    img, d = canvas()
    band = "13.56 MHz" if is_nfc else "433.92 MHz"
    unit = "%" if is_nfc else "dBm"
    draw_header(d, band)

    # phase pill
    nm = "BASELINE" if phase == 0 else "SHIELDED"
    sub = "open air" if phase == 0 else "in pouch"
    pw = tw(nm, f_pri)
    rbox(d, 2, 13, pw + 7, 12, 2)
    text(d, 6, 23, nm, f_pri, BG)
    text(d, 126, 23, sub, f_sec, anchor="rs")

    # live meter
    draw_bar(d, 2, 30, 70, 12, level, peak)

    if signal_ok or live != 0:
        draw_big_value(d, 126, 47, live, unit)
    else:
        text(d, 126, 40, "waiting", f_sec, anchor="rs")
        text(d, 126, 48, "for signal", f_sec, anchor="rs")

    if phase == 1 and base is not None:
        text(d, 2, 51, f"BASE {base} {unit}", f_sec)

    # action strip
    box(d, 0, 53, 128, 11)
    hint = (
        "Peak captured"
        if signal_ok
        else ("In reader field" if is_nfc else "Press remote")
    )
    text(d, 3, 62, hint, f_sec, BG)
    text(d, 125, 62, "OK lock", f_sec, BG, anchor="rs")
    save(img, name)


def render_verdict(
    name,
    *,
    is_nfc,
    base_v,
    shield_v,
    base_n,
    shield_n,
    atten,
    floored,
    letter,
    word,
    pips,
):
    img, d = canvas()
    band = "13.56 MHz" if is_nfc else "433.92 MHz"
    unit = "%" if is_nfc else "dB"  # a difference of dBm readings is dB
    draw_header(d, band)

    text(d, 2, 22, "OPEN", f_sec)
    draw_bar(d, 30, 15, 68, 8, base_n)
    text(d, 126, 22, str(base_v), f_sec, anchor="rs")

    text(d, 2, 32, "BAG", f_sec)
    draw_bar(d, 30, 25, 68, 8, shield_n)
    text(d, 126, 32, str(shield_v), f_sec, anchor="rs")

    s = str(max(atten, 0))
    nx = 2
    if floored:
        text(d, 2, 52, ">=", f_sec)
        nx = 15
    text(d, nx, 53, s, f_big)
    nw = tw(s, f_big)
    text(d, nx + nw + 2, 52, unit, f_sec)
    text(d, 92, 52, "BLOCKED" if is_nfc else "ATTEN", f_sec, anchor="rs")

    rframe(d, 96, 34, 31, 18, 3)
    text(d, 111, 43, letter, f_pri, anchor="ms")

    box(d, 0, 53, 128, 11)
    text(d, 3, 62, word, f_sec, BG)
    ww = tw(word, f_sec)
    draw_pips(d, 6 + ww, 57, pips, BG)
    text(d, 125, 62, "OK retest", f_sec, BG, anchor="rs")
    save(img, name)


def render_hunt(name, *, band, rssi, floor, level, peak_norm, history):
    """Mirrors views/hunt_view.c."""
    img, d = canvas()
    text(d, 2, 9, "LEAK HUNT", f_sec)
    text(d, 126, 9, band, f_sec, anchor="rs")
    line(d, 0, 11, 127, 11)

    margin = max(0, rssi - floor)
    word = (
        "BLAZING"
        if margin >= 30
        else (
            "HOT"
            if margin >= 18
            else "WARM" if margin >= 10 else "COOL" if margin >= 4 else "COLD"
        )
    )
    d.text((L(64), L(20)), word, font=f_pri, fill=FG, anchor="mm")
    if margin >= 30:
        w = tw(word, f_pri)
        frame(d, 64 - w / 2 - 4, 13, w + 8, 14)

    # live bar + peak tick
    frame(d, 2, 29, 124, 11)
    fw = (120 * min(level, 100)) // 100
    if fw > 0:
        box(d, 4, 31, fw, 7)
    px = 4 + (120 * min(peak_norm, 100)) // 100
    line(d, px, 28, px, 40)

    # rolling trace
    for k in range(62):
        v = history[(len(history) - 1 - k) % len(history)]
        x = 126 - k * 2
        y = 51 - (v * 11) // 100
        if y < 51:
            line(d, x, 51, x, y, FG, w=S)
        else:
            d.rectangle([L(x), L(51), L(x) + S - 1, L(51) + S - 1], fill=FG)

    box(d, 0, 53, 128, 11)
    text(d, 3, 62, f"{rssi} dBm  +{margin}", f_sec, BG)
    text(d, 125, 62, "OK reset", f_sec, BG, anchor="rs")
    save(img, name)


def render_results():
    """The stock text-scroll widget, showing the saved log."""
    img, d = canvas()
    rows = [
        ("A  >=54 dB", True),
        ("433.92", False),
        ("07-18 14:32  (-42 to -96)", False),
        ("F  7 dB", True),
        ("433.92", False),
        ("07-18 14:29  (-42 to -49)", False),
        ("A+  99 %", True),
    ]
    y = 9
    for txt, bold in rows:
        text(d, 2, y, txt, f_pri if bold else f_sec)
        y += 8
    # scrollbar
    box(d, 125, 0, 3, 64, (200, 100, 0))
    box(d, 125, 0, 3, 26)
    save(img, "screen_results.png")


def render_menu():
    img, d = canvas()
    text(d, 4, 11, "Faraday", f_pri)
    line(d, 0, 14, 127, 14)
    items = [
        "Test Sub-GHz (key fob)",
        "Test NFC (card)",
        "Leak hunt (Sub-GHz)",
        "Saved results",
    ]
    ROW_H = 12
    for i, it in enumerate(items):
        y = 15 + i * ROW_H
        col = FG
        if i == 0:
            box(d, 0, y, 124, ROW_H)
            col = BG
        text(d, 4, y + 9, it, f_sec, col)
    box(d, 125, 15, 3, 12)
    save(img, "screen_menu.png")


def render_settings():
    img, d = canvas()
    text(d, 4, 11, "Settings", f_pri)
    line(d, 0, 14, 127, 14)
    rows = [
        ("Sub-GHz band", "433.92", True),
        ("Sound", "ON", False),
        ("LED", "ON", False),
    ]
    ROW_H = 12
    for i, (k, v, sel) in enumerate(rows):
        y = 15 + i * ROW_H
        col = FG
        if sel:
            box(d, 0, y, 124, ROW_H)
            col = BG
        text(d, 4, y + 9, k, f_sec, col)
        text(d, 121, y + 9, v, f_sec, col, anchor="rs")
    box(d, 125, 15, 3, 12)
    save(img, "screen_settings.png")


if __name__ == "__main__":
    # Sub-GHz: fob in the open, carrier landing hard
    render_capture(
        "screen_baseline.png",
        is_nfc=False,
        phase=0,
        level=72,
        peak=78,
        live=-42,
        signal_ok=True,
    )
    # Sub-GHz: fob sealed in the pouch, barely anything left
    render_capture(
        "screen_shielded.png",
        is_nfc=False,
        phase=1,
        level=6,
        peak=9,
        live=-94,
        signal_ok=False,
        base=-42,
    )
    # The money screen: a pouch that works
    render_verdict(
        "screen_verdict.png",
        is_nfc=False,
        base_v=-42,
        shield_v=-96,
        base_n=78,
        shield_n=6,
        atten=54,
        floored=True,
        letter="A",
        word="STRONG",
        pips=4,
    )
    # A pouch that does not
    render_verdict(
        "screen_fail.png",
        is_nfc=False,
        base_v=-42,
        shield_v=-49,
        base_n=78,
        shield_n=70,
        atten=7,
        floored=False,
        letter="F",
        word="OPEN",
        pips=0,
    )
    # NFC capture against a reader field
    render_capture(
        "screen_nfc.png",
        is_nfc=True,
        phase=0,
        level=86,
        peak=91,
        live=86,
        signal_ok=True,
    )
    # Leak hunt: swept onto the seam where the pouch is escaping
    HUNT_HIST = [
        4,
        6,
        3,
        8,
        5,
        2,
        7,
        4,
        9,
        6,
        3,
        8,
        5,
        11,
        7,
        4,
        9,
        6,
        14,
        8,
        5,
        12,
        18,
        9,
        6,
        15,
        22,
        11,
        7,
        19,
        28,
        14,
        9,
        24,
        35,
        17,
        11,
        30,
        44,
        21,
        13,
        38,
        55,
        26,
        16,
        47,
        68,
        32,
        20,
        58,
        82,
        39,
        24,
        71,
        95,
        47,
        29,
        84,
        99,
        56,
        34,
        92,
        100,
        62,
    ]
    render_hunt(
        "screen_hunt.png",
        band="433.92 MHz",
        rssi=-58,
        floor=-96,
        level=62,
        peak_norm=78,
        history=HUNT_HIST,
    )
    render_results()
    render_menu()
    render_settings()

    names = (
        "screen_baseline.png",
        "screen_shielded.png",
        "screen_verdict.png",
        "screen_fail.png",
        "screen_nfc.png",
        "screen_hunt.png",
        "screen_results.png",
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

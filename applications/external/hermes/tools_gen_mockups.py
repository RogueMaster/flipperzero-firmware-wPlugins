#!/usr/bin/env python3
"""Render 128x64 mockups of Hermes' screens for the README.

Every layout constant here is copied from the C that actually draws the screen
(views/*.c), so these are renderings of the real geometry rather than an
artist's impression. The detect scope even plays a genuine 8N1 waveform through
the same maths the firmware uses, so the trace on screen is real data.

Amber LCD look, dark ink, upscaled NEAREST for crisp pixels, plus a bezel.
"""

from PIL import Image, ImageDraw, ImageFont
import os

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

ORANGE = (255, 159, 12)
INK = (26, 18, 2)
BEZEL = (18, 18, 22)
BEZEL_HI = (44, 44, 52)
BACKDROP = (13, 15, 22)

SCALE = 7
W, H = 128, 64

FB = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
FR = "/System/Library/Fonts/Supplemental/Arial.ttf"
FMONO = "/System/Library/Fonts/Menlo.ttc"

PRIM = ImageFont.truetype(FB, 9)
SEC = ImageFont.truetype(FR, 8)
BIG = ImageFont.truetype(FB, 19)
# 10px Menlo advances exactly 6px per glyph, so 21 columns land in 126px -
# the same grid Flipper's FontKeyboard uses.
MONO = ImageFont.truetype(FMONO, 10)


def screen():
    return Image.new("RGB", (W, H), ORANGE)


# The firmware positions every string by its *baseline* (canvas_draw_str takes
# y as the baseline). These wrappers take the same coordinates the C does, so
# the mockups inherit the real layout instead of drifting from it.
def ltext(d, x, baseline, s, font, fill=INK):
    d.text((x, baseline), s, font=font, fill=fill, anchor="ls")


def rtext(d, rx, baseline, s, font, fill=INK):
    d.text((rx, baseline), s, font=font, fill=fill, anchor="rs")


def ctext(d, cx, baseline, s, font, fill=INK):
    d.text((cx, baseline), s, font=font, fill=fill, anchor="ms")


def finish(img, name):
    up = img.resize((W * SCALE, H * SCALE), Image.NEAREST)
    pad = 20
    canvas = Image.new("RGB", (W * SCALE + pad * 2, H * SCALE + pad * 2), BEZEL)
    d = ImageDraw.Draw(canvas)
    d.rounded_rectangle(
        [6, 6, canvas.width - 6, canvas.height - 6],
        radius=16,
        outline=BEZEL_HI,
        width=3,
    )
    canvas.paste(up, (pad, pad))
    path = os.path.join(OUT, name)
    canvas.save(path)
    print("wrote", path)
    return path


# --------------------------------------------------------------- waveform ----
# Mirrors helpers/autobaud.c + views/detect_view.c so the trace is honest.

CPU_HZ = 64_000_000
WAVE_PX_PER_BIT = 3
WAVE_MAX_SEG_PX = 30
WAVE_X0, WAVE_X1 = 4, 123
WAVE_Y_HIGH, WAVE_Y_LOW = 17, 33


def uart_segments(text, baud):
    """Turn a string into (duration_cycles, level) runs, exactly as the pin would."""
    cycles_per_bit = CPU_HZ / baud
    bits = []
    for ch in text:
        bits.append(0)  # start
        bits += [(ord(ch) >> i) & 1 for i in range(8)]  # LSB first
        bits.append(1)  # stop
        bits += [1, 1]  # idle

    segs, run, cur = [], 0, bits[0]
    for b in bits:
        if b == cur:
            run += 1
        else:
            segs.append((int(run * cycles_per_bit), cur == 1))
            cur, run = b, 1
    segs.append((int(run * cycles_per_bit), cur == 1))
    return segs


def draw_wave(d, segs, bit_time):
    widths = [
        max(1, min(WAVE_MAX_SEG_PX, int(delta * WAVE_PX_PER_BIT / bit_time)))
        for delta, _ in segs
    ]

    # Fill from the newest backwards, then draw forward - same as the firmware.
    width = WAVE_X1 - WAVE_X0
    used, first = 0, len(segs)
    while first > 0 and used + widths[first - 1] <= width:
        used += widths[first - 1]
        first -= 1

    x = WAVE_X0 + (width - used)
    for i in range(first, len(segs)):
        w = widths[i]
        y = WAVE_Y_HIGH if segs[i][1] else WAVE_Y_LOW
        d.line([(x, y), (x + w - 1, y)], fill=INK)
        if i + 1 < len(segs):
            y_next = WAVE_Y_HIGH if segs[i + 1][1] else WAVE_Y_LOW
            if y_next != y:
                d.line([(x + w - 1, WAVE_Y_HIGH), (x + w - 1, WAVE_Y_LOW)], fill=INK)
        x += w


# ------------------------------------------------------------------ menu -----
def m_menu():
    img = screen()
    d = ImageDraw.Draw(img)
    ctext(d, 64, 9, "Hermes", PRIM)
    d.line([(0, 12), (128, 12)], fill=INK)

    items = ["Detect Baud", "Manual Console", "Self Test", "Wiring Guide"]
    y = 15
    for i, it in enumerate(items):
        if i == 0:
            d.rounded_rectangle([2, y - 1, 125, y + 11], radius=3, fill=INK)
            ltext(d, 7, y + 9, it, SEC, fill=ORANGE)
        else:
            ltext(d, 7, y + 9, it, SEC)
        y += 12
    return finish(img, "screen_menu.png")


# ---------------------------------------------------------------- detect -----
def m_detect():
    img = screen()
    d = ImageDraw.Draw(img)

    ltext(d, 2, 8, "Listening", PRIM)
    rtext(d, 126, 8, "RX=14 TX=13", SEC)
    d.line([(0, 10), (127, 10)], fill=INK)

    segs = uart_segments("root@openwrt:/# ", 115200)
    draw_wave(d, segs, CPU_HZ // 115200)

    ltext(d, 2, 48, "412 edges  ~115200 bd", SEC)

    # elements_progress_bar(canvas, 2, 53, 124, 0.80)
    d.rounded_rectangle([2, 53, 126, 61], radius=3, outline=INK, width=1)
    d.rounded_rectangle([4, 55, 4 + int(120 * 0.80), 59], radius=2, fill=INK)
    return finish(img, "screen_detect.png")


# ---------------------------------------------------------------- result -----
def m_result():
    img = screen()
    d = ImageDraw.Draw(img)

    ltext(d, 2, 8, "BEST MATCH", SEC)
    rtext(d, 126, 8, "verified", SEC)
    d.line([(0, 10), (127, 10)], fill=INK)

    ltext(d, 2, 30, "115200", BIG)
    rtext(d, 126, 30, "8N1", PRIM)

    # result_draw_confidence: frame at x=2 y=RESULT_BAR_Y w=88 h=7
    d.rectangle([2, 32, 90, 39], outline=INK, width=1)
    d.rectangle([3, 33, 3 + int(86 * 0.97), 38], fill=INK)
    ltext(d, 94, 38, "97%", SEC)

    ltext(d, 2, 48, "128 B  94% text", SEC)
    rtext(d, 126, 48, "the modern default", SEC)

    # result_draw_ladder: chip height encodes confidence
    for i, conf in enumerate([97, 41, 22, 8]):
        x = 2 + i * 10
        h = max(2, 2 + int(9 * conf / 100))
        if i == 0:
            d.rectangle([x, 62 - h, x + 7, 62], fill=INK)
        else:
            d.rectangle([x, 62 - h, x + 7, 62], outline=INK, width=1)

    rtext(d, 126, 62, "OK: console", SEC)
    return finish(img, "screen_result.png")


# --------------------------------------------------------------- console -----
def m_console():
    img = screen()
    d = ImageDraw.Draw(img)

    d.rectangle([0, 0, 128, 9], fill=INK)
    # REC dot: a capture is being written to the card
    d.ellipse([2, 2, 6, 6], fill=ORANGE)
    ltext(d, 9, 7, "115200 8N1", SEC, fill=ORANGE)
    rtext(d, 126, 7, "TXT  RW", SEC, fill=ORANGE)

    # CONSOLE_TOP=11, CONSOLE_ROW_H=9, drawn at (1, y + 7)
    lines = [
        "U-Boot 2019.04",
        "DRAM:  128 MiB",
        "Net:   eth0",
        "Hit any key to stop",
        "autoboot:  0",
        "root@openwrt:/# _",
    ]
    for r, line in enumerate(lines):
        ltext(d, 1, 11 + r * 9 + 7, line, MONO)
    return finish(img, "screen_console.png")


# ---------------------------------------------------------------- wiring -----
def m_wiring():
    img = screen()
    d = ImageDraw.Draw(img)

    ltext(d, 2, 8, "Wiring", PRIM)
    rtext(d, 126, 8, "1/2", SEC)
    d.line([(0, 10), (127, 10)], fill=INK)

    XL, XR = 28, 100
    Y_RX, Y_TX, Y_GND = 28, 38, 48
    DROP = 3

    d.rounded_rectangle([2, 20, 28, 56], radius=3, outline=INK, width=1)
    d.rounded_rectangle([100, 20, 126, 56], radius=3, outline=INK, width=1)
    ltext(d, 2, 18, "FLIPPER", SEC)
    rtext(d, 126, 18, "TARGET", SEC)

    # RX <- TX, with the data crawling along it toward the Flipper
    d.line([(XL, Y_RX), (XR, Y_RX)], fill=INK)
    for x in range(XR - 4, XL, -12):
        d.rectangle([x - 1, Y_RX - 1, x + 1, Y_RX + 1], fill=INK)
    rtext(d, 26, Y_RX + DROP, "14", SEC)
    ltext(d, 102, Y_RX + DROP, "TX", SEC)

    # TX -> RX, dashed: optional until you type
    for x in range(XL, XR, 4):
        d.line([(x, Y_TX), (x + 1, Y_TX)], fill=INK)
    rtext(d, 26, Y_TX + DROP, "13", SEC)
    ltext(d, 102, Y_TX + DROP, "RX", SEC)

    # GND, drawn heavy because forgetting it is the number one failure
    d.line([(XL, Y_GND), (XR, Y_GND)], fill=INK)
    d.line([(XL, Y_GND + 1), (XR, Y_GND + 1)], fill=INK)
    rtext(d, 26, Y_GND + DROP, "8", SEC)
    ltext(d, 102, Y_GND + DROP, "GND", SEC)

    for y in (Y_RX, Y_TX, Y_GND):
        d.ellipse([XL - 1, y - 1, XL + 1, y + 1], fill=INK)
        d.ellipse([XR - 1, y - 1, XR + 1, y + 1], fill=INK)

    # The firmware asks for AlignBottom at y=63, which reserves room for the
    # descender; a plain baseline here would clip the 'y'.
    ctext(d, 64, 61, "RX and TX always cross", SEC)
    return finish(img, "screen_wiring.png")


def m_selftest():
    """The result state: which rates survived the cable."""
    img = screen()
    d = ImageDraw.Draw(img)

    ltext(d, 2, 8, "Self Test", PRIM)
    rtext(d, 126, 8, "loopback", SEC)
    d.line([(0, 10), (127, 10)], fill=INK)

    ltext(d, 2, 22, "Marginal at speed", PRIM)

    # per-rate rows at SELFTEST_ROW_TOP + i*SELFTEST_ROW_H, as the C does
    rows = [(9600, True), (115200, True), (460800, False), (921600, False)]
    for i, (baud, ok) in enumerate(rows):
        y = 30 + i * 8
        ltext(d, 8, y, str(baud), SEC)
        if ok:
            d.line([(2, y - 3), (4, y - 1)], fill=INK)
            d.line([(4, y - 1), (6, y - 5)], fill=INK)
            ltext(d, 60, y, "echoed", SEC)
        else:
            d.line([(2, y - 5), (6, y - 1)], fill=INK)
            d.line([(6, y - 5), (2, y - 1)], fill=INK)
            ltext(d, 60, y, "0/10 back", SEC)

    ltext(d, 2, 62, "Shorten the wires", SEC)
    rtext(d, 126, 62, "OK: retry", SEC)
    return finish(img, "screen_selftest.png")


def m_rules():
    img = screen()
    d = ImageDraw.Draw(img)

    ltext(d, 2, 8, "Rules", PRIM)
    rtext(d, 126, 8, "2/2", SEC)
    d.line([(0, 10), (127, 10)], fill=INK)

    ltext(d, 2, 21, "3.3V logic only", PRIM)
    ltext(d, 2, 32, "RS-232 (+/-12V) kills the", SEC)
    ltext(d, 2, 41, "Flipper. Use a MAX3232.", SEC)
    ltext(d, 2, 53, "Ground first. Never wire 5V", SEC)
    ltext(d, 2, 62, "into a self-powered board.", SEC)
    return finish(img, "screen_rules.png")


# ---------------------------------------------------------------- strips -----
def strip(paths, name, cols=None):
    imgs = [Image.open(p) for p in paths]
    cols = cols or len(imgs)
    rows = (len(imgs) + cols - 1) // cols
    gap = 24

    cw, ch = imgs[0].width, imgs[0].height
    canvas = Image.new(
        "RGB", (cols * cw + gap * (cols - 1), rows * ch + gap * (rows - 1)), BACKDROP
    )
    for i, im in enumerate(imgs):
        canvas.paste(im, ((i % cols) * (cw + gap), (i // cols) * (ch + gap)))

    path = os.path.join(OUT, name)
    canvas.save(path)
    print("wrote", path)


if __name__ == "__main__":
    menu = m_menu()
    detect = m_detect()
    result = m_result()
    console = m_console()
    wiring = m_wiring()
    rules = m_rules()
    selftest = m_selftest()

    strip([detect, result, console], "screens.png")
    strip(
        [menu, detect, result, console, selftest, wiring],
        "screens_all.png",
        cols=3,
    )

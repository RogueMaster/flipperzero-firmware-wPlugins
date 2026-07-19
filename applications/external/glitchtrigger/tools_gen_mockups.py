#!/usr/bin/env python3
"""Render 128x64 Flipper-style mockups of Glitch Trigger's screens for the
README. Amber LCD look, dark ink, upscaled NEAREST for a crisp pixel feel + a
bezel. These mirror what the on-device views actually draw."""
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


def f(path, px):
    return ImageFont.truetype(path, px)


PRIM = f(FB, 9)
SEC = f(FR, 7)


def screen():
    return Image.new("RGB", (W, H), ORANGE)


def ctext(d, cx, y, s, font, fill=INK):
    w = d.textlength(s, font=font)
    d.text((cx - w / 2, y), s, font=font, fill=fill)


def rtext(d, rx, y, s, font, fill=INK):
    w = d.textlength(s, font=font)
    d.text((rx - w, y), s, font=font, fill=fill)


def chip(d, x, y, s, font, filled):
    w = d.textlength(s, font=font) + 6
    if filled:
        d.rounded_rectangle([x, y, x + w, y + 11], radius=2, fill=INK)
        d.text((x + 3, y + 1), s, font=font, fill=ORANGE)
    else:
        d.rounded_rectangle([x, y, x + w, y + 11], radius=2, outline=INK, width=1)
        d.text((x + 3, y + 1), s, font=font, fill=INK)
    return w


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


# ------------------------------------------------------------------ 1. menu
def m_menu():
    img = screen()
    d = ImageDraw.Draw(img)
    ctext(d, 64, 0, "Glitch Trigger", PRIM)
    d.line([(0, 13), (128, 13)], fill=INK)
    items = ["Trigger", "Configure", "Sweep", "Wiring"]
    y = 16
    for i, it in enumerate(items):
        if i == 0:
            d.rounded_rectangle([2, y - 1, 125, y + 11], radius=3, fill=INK)
            d.text((7, y + 1), it, font=SEC, fill=ORANGE)
        else:
            d.text((7, y + 1), it, font=SEC, fill=INK)
        y += 12
    return finish(img, "screen_menu.png")


# --------------------------------------------------------------- 2. trigger
def waveform(d, pulses, active_high, flash):
    LEFT, RIGHT, HI, LO = 4, 124, 18, 34
    TRIG, D0, D1, PW, GAP = 9, 12, 46, 8, 4
    idle = LO if active_high else HI
    act = HI if active_high else LO
    # trigger tick
    d.line([(TRIG, HI - 3), (TRIG, LO + 3)], fill=INK)
    d.line([(TRIG - 2, HI - 1), (TRIG, HI - 3)], fill=INK)
    d.line([(TRIG + 2, HI - 1), (TRIG, HI - 3)], fill=INK)
    # idle rail
    d.line([(LEFT, idle), (D0, idle)], fill=INK)
    for x in range(D0, D1, 3):
        d.point((x, idle), fill=INK)
    # pulses
    x = D1
    drawn = min(pulses, 4)
    for i in range(drawn):
        d.line([(x, idle), (x, act)], fill=INK)
        d.line([(x, act), (x + PW, act)], fill=INK)
        if flash:
            d.line([(x, act + 1), (x + PW, act + 1)], fill=INK)
        x += PW
        d.line([(x, act), (x, idle)], fill=INK)
        if i + 1 < drawn:
            d.line([(x, idle), (x + GAP, idle)], fill=INK)
            x += GAP
    d.line([(x, idle), (RIGHT, idle)], fill=INK)
    if flash:
        sx = D1 + PW // 2
        d.ellipse([sx - 1, act - 4, sx + 1, act - 2], fill=INK)
        d.line([(sx - 3, act - 6), (sx + 3, act - 6)], fill=INK)


def m_trigger():
    img = screen()
    d = ImageDraw.Draw(img)
    d.text((2, 1), "Glitch", font=PRIM, fill=INK)
    cw = d.textlength("Manual", font=SEC) + 6
    chip(d, 126 - cw, 1, "Manual", SEC, False)
    d.line([(0, 13), (128, 13)], fill=INK)
    waveform(d, 1, True, True)
    ctext(d, 64, 37, "100 us   500 ns   x1", SEC)
    chip(d, 2, 45, "ARMED", SEC, True)
    rtext(d, 126, 47, "shots 7", SEC)
    ctext(d, 64, 55, "OK: FIRE   Back: safe", SEC)
    return finish(img, "screen_trigger.png")


# ----------------------------------------------------------------- 3. sweep
def m_sweep():
    img = screen()
    d = ImageDraw.Draw(img)
    d.text((2, 1), "Sweep", font=PRIM, fill=INK)
    cw = d.textlength("RUN", font=SEC) + 6
    chip(d, 126 - cw, 1, "RUN", SEC, True)
    d.line([(0, 14), (128, 14)], fill=INK)
    d.text((2, 16), "width", font=PRIM, fill=INK)
    rtext(d, 126, 16, "2.0 us", PRIM)
    # progress bar
    d.rounded_rectangle([2, 31, 126, 38], radius=2, outline=INK, width=1)
    d.rectangle([3, 32, 3 + 68, 37], fill=INK)
    d.text((2, 40), "125 ns..5.0 us", font=SEC, fill=INK)
    rtext(d, 126, 40, "15/40", SEC)
    d.text((2, 48), "hit @ 2.0 us (1)", font=SEC, fill=INK)
    ctext(d, 64, 55, "OK:hit  <:pause  >:restart", SEC)
    return finish(img, "screen_sweep.png")


# ---------------------------------------------------------------- 4. wiring
def m_wiring():
    img = screen()
    d = ImageDraw.Draw(img)
    d.text((2, 0), "Wiring", font=PRIM, fill=INK)
    d.line([(0, 12), (128, 12)], fill=INK)
    by, bh = 22, 22

    def box(x, w, title):
        d.rounded_rectangle([x, by, x + w, by + bh], radius=2, outline=INK, width=1)
        ctext(d, x + w / 2, by - 9, title, SEC)

    box(2, 40, "FLIPPER")
    box(55, 26, "SWITCH")
    box(94, 32, "TARGET")
    d.text((5, by + 2), "P2 glt", font=SEC, fill=INK)
    d.text((5, by + 11), "P6 trg", font=SEC, fill=INK)
    # mosfet glyph
    gx = 62
    d.line([(55, by + 11), (gx, by + 11)], fill=INK)
    d.line([(gx, by + 4), (gx, by + bh - 4)], fill=INK)
    d.line([(gx + 2, by + 5), (gx + 2, by + bh - 5)], fill=INK)
    d.line([(gx + 2, by + 6), (78, by + 6)], fill=INK)
    d.line([(gx + 2, by + bh - 6), (78, by + bh - 6)], fill=INK)
    d.text((97, by + 2), "VCC", font=SEC, fill=INK)
    d.text((97, by + 11), "GND", font=SEC, fill=INK)
    d.line([(42, by + 4), (55, by + 4)], fill=INK)
    d.line([(78, by + 6), (94, by + 6)], fill=INK)
    d.line([(12, by + bh + 3), (116, by + bh + 3)], fill=INK)
    ctext(d, 64, 54, "3V3 logic only - never", SEC)
    ctext(d, 64, 62, "wire a rail to a GPIO", SEC)
    return finish(img, "screen_wiring.png")


# --------------------------------------------------------------- 5. configure
def m_params():
    img = screen()
    d = ImageDraw.Draw(img)
    rows = [("Delay", "100 us"), ("Width", "500 ns"), ("Pulses", "1"),
            ("Gap", "10 us"), ("Polarity", "Active-High")]
    y = 2
    for i, (k, v) in enumerate(rows):
        if i == 1:
            d.rectangle([0, y - 1, 128, y + 11], fill=INK)
            d.text((4, y + 1), k, font=SEC, fill=ORANGE)
            rtext(d, 124, y + 1, "<%s>" % v, SEC, fill=ORANGE)
        else:
            d.text((4, y + 1), k, font=SEC, fill=INK)
            rtext(d, 124, y + 1, v, SEC, fill=INK)
        y += 12
    return finish(img, "screen_params.png")


def strip():
    names = ["screen_menu.png", "screen_trigger.png", "screen_sweep.png",
             "screen_wiring.png", "screen_params.png"]
    imgs = [Image.open(os.path.join(OUT, n)) for n in names]
    gap = 16
    tw = sum(i.width for i in imgs) + gap * (len(imgs) - 1)
    th = max(i.height for i in imgs)
    canvas = Image.new("RGB", (tw, th), (12, 12, 15))
    x = 0
    for im in imgs:
        canvas.paste(im, (x, (th - im.height) // 2))
        x += im.width + gap
    path = os.path.join(OUT, "screens.png")
    canvas.save(path)
    print("wrote", path)


if __name__ == "__main__":
    m_menu()
    m_trigger()
    m_sweep()
    m_wiring()
    m_params()
    strip()

#!/usr/bin/env python3
"""Render the Trident GitHub banner + social-preview card.

Theme: a glowing three-pronged trident (one prong per radio: ESP32 / NRF24 /
CC1101) rising through three concentric signal arcs on a deep teal-navy field,
cyan + gold accents. Supersampled for smooth edges.
"""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import os, random

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

BLACK_F = "/System/Library/Fonts/Supplemental/Arial Black.ttf"
BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"

# palette
BG_TOP = (7, 14, 20)
BG_BOT = (12, 26, 36)
CYAN = (78, 224, 232)
TEAL = (46, 176, 190)
GOLD = (240, 196, 108)
WHITE = (238, 246, 248)
GRAY = (150, 166, 172)
DIM = (34, 52, 60)

SS = 2  # supersample


def font(path, px):
    try:
        return ImageFont.truetype(path, px)
    except OSError:
        return ImageFont.truetype(BOLD, px)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def vgradient(w, h, top, bot):
    img = Image.new("RGB", (w, h), top)
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        d.line([(0, y), (w, y)], fill=lerp(top, bot, t))
    return img


def signal_arcs(d, cx, cy, col, count=3, base=60, gap=44, width=9, spread=170):
    for i in range(count):
        r = base + i * gap
        fade = lerp(col, BG_BOT, i / max(1, count))
        d.arc(
            [cx - r, cy - r, cx + r, cy + r],
            start=-90 - spread / 2,
            end=-90 + spread / 2,
            fill=fade,
            width=width,
        )


def trident(d, cx, cy, s, col, w):
    """Three-pronged trident centred at (cx, cy). s = unit scale, w = stroke."""
    tip_y = cy - int(2.3 * s)
    bar_y = cy - int(0.5 * s)
    bot_y = cy + int(2.2 * s)
    off = int(0.95 * s)

    # crossbar
    d.line([(cx - s, bar_y), (cx + s, bar_y)], fill=col, width=w)
    # shaft
    d.line([(cx, bar_y), (cx, bot_y)], fill=col, width=w)
    # three prongs
    for px in (cx - off, cx, cx + off):
        d.line([(px, bar_y), (px, tip_y)], fill=col, width=w)
        # spearhead tip
        d.polygon(
            [(px, tip_y - int(0.5 * s)), (px - int(0.28 * s), tip_y), (px + int(0.28 * s), tip_y)],
            fill=col,
        )
    # decorative base cross + finial
    d.line([(cx - int(0.5 * s), bot_y - int(0.4 * s)),
            (cx + int(0.5 * s), bot_y - int(0.4 * s))], fill=col, width=w)
    d.ellipse([cx - int(0.28 * s), bot_y - int(0.28 * s),
               cx + int(0.28 * s), bot_y + int(0.28 * s)], fill=col)


def glow(size, fn):
    layer = Image.new("RGB", size, (0, 0, 0))
    fn(ImageDraw.Draw(layer))
    return layer.filter(ImageFilter.GaussianBlur(radius=16 * SS))


def compose(w, h, title_px, tag_px, chip_px, wm_y, tag_y, chips_y, emblem):
    W, H = w * SS, h * SS
    img = vgradient(W, H, BG_TOP, BG_BOT)
    d = ImageDraw.Draw(img)

    # starfield
    random.seed(3)
    for _ in range(110):
        x, y = random.randint(0, W), random.randint(0, H)
        r = random.choice([1, 1, 2]) * SS
        d.ellipse([x - r, y - r, x + r, y + r], fill=lerp(DIM, WHITE, random.random() * 0.3))

    ex, ey = int(emblem[0] * SS), int(emblem[1] * SS)
    es = int(46 * SS)
    ew = 11 * SS

    # glow pass (arcs + trident)
    def gfn(dd):
        signal_arcs(dd, ex, ey - int(es * 1.4), CYAN, 3, 66 * SS, 50 * SS, 12 * SS)
        trident(dd, ex, ey, es, lerp(CYAN, GOLD, 0.5), ew + 4 * SS)

    img = Image.composite(
        Image.new("RGB", (W, H), (255, 255, 255)),
        img,
        glow((W, H), gfn).convert("L").point(lambda v: min(v, 130)),
    )
    d = ImageDraw.Draw(img)

    # crisp arcs + trident
    signal_arcs(d, ex, ey - int(es * 1.4), TEAL, 3, 66 * SS, 50 * SS, 10 * SS)
    trident(d, ex, ey, es, WHITE, ew)
    trident(d, ex, ey, es, GOLD, max(2 * SS, ew - 7 * SS))

    # wordmark
    f_title = font(BLACK_F, title_px * SS)
    f_tag = font(BOLD, tag_px * SS)
    f_chip = font(MONO, chip_px * SS)

    tx = 74 * SS
    d.text((tx + 3 * SS, wm_y * SS + 3 * SS), "TRIDENT", font=f_title, fill=(0, 0, 0))
    d.text((tx, wm_y * SS), "TRIDENT", font=f_title, fill=WHITE)
    d.text((tx + 2 * SS, tag_y * SS), "3-in-1 RF controller for Flipper Zero",
           font=f_tag, fill=CYAN)

    chips = ["ESP32 Wi-Fi/BT", "NRF24 2.4GHz", "CC1101 Sub-GHz"]
    cx = tx + 2 * SS
    for c in chips:
        bb = d.textbbox((0, 0), c, font=f_chip)
        cw = bb[2] - bb[0]
        padx = 12 * SS
        d.rounded_rectangle(
            [cx, chips_y * SS, cx + cw + padx * 2, chips_y * SS + (chip_px + 12) * SS],
            radius=8 * SS, outline=GOLD, width=2 * SS,
        )
        d.text((cx + padx, (chips_y + 6) * SS), c, font=f_chip, fill=WHITE)
        cx += cw + padx * 2 + 12 * SS

    return img.resize((w, h), Image.LANCZOS)


def main():
    compose(1280, 360, 100, 28, 16, 106, 214, 262, (1058, 200)).save(
        os.path.join(OUT, "banner.png"))
    print("wrote banner.png")
    compose(1280, 640, 128, 34, 19, 236, 388, 452, (1040, 356)).save(
        os.path.join(OUT, "social-preview.png"))
    print("wrote social-preview.png")


if __name__ == "__main__":
    main()

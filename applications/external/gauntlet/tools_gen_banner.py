#!/usr/bin/env python3
"""Render the Gauntlet GitHub banner + social-preview card.
CTF theme: deep space-navy, a terminal-green glow, a flag planted on a starburst
(capture the flag), category chips (CIPHER / RFID / IR) and a flag{} token.
Supersampled for crisp edges."""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import os, math

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
BLACK_F = "/System/Library/Fonts/Supplemental/Arial Black.ttf"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
REG = "/System/Library/Fonts/Supplemental/Arial.ttf"

BG_TOP = (9, 11, 17)
BG_BOT = (15, 19, 30)
GREEN = (58, 226, 148)
CYAN = (56, 200, 224)
AMBER = (255, 150, 44)
MAGENTA = (232, 88, 160)
WHITE = (238, 244, 250)
GRAY = (150, 162, 184)
DIM = (40, 48, 70)

SS = 2  # supersample


def font(path, px):
    try:
        return ImageFont.truetype(path, px)
    except OSError:
        return ImageFont.truetype(BOLD, px)


def vgradient(w, h):
    img = Image.new("RGB", (w, h), BG_TOP)
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        r = int(BG_TOP[0] + (BG_BOT[0] - BG_TOP[0]) * t)
        g = int(BG_TOP[1] + (BG_BOT[1] - BG_TOP[1]) * t)
        b = int(BG_TOP[2] + (BG_BOT[2] - BG_TOP[2]) * t)
        d.line([(0, y), (w, y)], fill=(r, g, b))
    return img


def dot_grid(d, w, h, step, col):
    for y in range(0, h, step):
        for x in range(0, w, step):
            d.point((x, y), fill=col)


def rrect(d, box, r, **kw):
    d.rounded_rectangle(box, radius=r, **kw)


def soft(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def draw_flag(d, x, y, h, col, lw):
    """A pennant flag planted on a pole."""
    d.line([(x, y - h), (x, y + h)], fill=col, width=lw)          # pole
    d.line([(x - h // 3, y + h), (x + h // 3, y + h)], fill=col, width=lw)  # base
    # triangular pennant
    fw = int(h * 1.15)
    d.polygon([(x + lw, y - h), (x + lw + fw, y - int(h * 0.6)),
               (x + lw, y - int(h * 0.2))], fill=col)


def draw_burst(d, cx, cy, r0, r1, col, lw, n=12):
    for i in range(n):
        a = i * (2 * math.pi / n)
        d.line([(cx + math.cos(a) * r0, cy + math.sin(a) * r0),
                (cx + math.cos(a) * r1, cy + math.sin(a) * r1)], fill=col, width=lw)


def chip(d, x, y, text, col, fnt):
    bb = d.textbbox((0, 0), text, font=fnt)
    tw, th = bb[2] - bb[0], bb[3] - bb[1]
    padx, pady = 15 * SS, 8 * SS
    rrect(d, [x, y, x + tw + padx * 2, y + th + pady * 2], r=(th + pady * 2) // 2,
          outline=col, width=3 * SS)
    d.text((x + padx - bb[0], y + pady - bb[1]), text, font=fnt, fill=col)
    return x + tw + padx * 2


def chip_row(d, cx, y, items, fnt, gap):
    """Measure a row of chips, then draw it centred on cx."""
    widths = []
    for text, _ in items:
        bb = d.textbbox((0, 0), text, font=fnt)
        widths.append(bb[2] - bb[0] + 30 * SS)  # + 2*padx
    total = sum(widths) + gap * (len(items) - 1)
    x = cx - total // 2
    for (text, col), wdt in zip(items, widths):
        chip(d, x, y, text, col, fnt)
        x += wdt + gap


def render(path, W, H, layout="wide"):
    w, h = W * SS, H * SS
    img = vgradient(w, h).convert("RGBA")

    grid = soft((w, h))
    dot_grid(ImageDraw.Draw(grid), w, h, 26 * SS, (255, 255, 255, 12))
    img.alpha_composite(grid)

    glow = soft((w, h))
    gd = ImageDraw.Draw(glow)
    if layout == "wide":
        gx, gy, scale = int(w * 0.80), int(h * 0.38), h
    else:
        gx, gy, scale = int(w * 0.5), int(h * 0.20), int(h * 0.52)

    # starburst + planted flag
    draw_burst(gd, gx, gy, int(scale * 0.14), int(scale * 0.26),
               (GREEN[0], GREEN[1], GREEN[2], 120), 4 * SS)
    draw_flag(gd, gx - int(scale * 0.02), gy + int(scale * 0.08), int(scale * 0.17),
              (GREEN[0], GREEN[1], GREEN[2], 230), 6 * SS)

    blur = glow.filter(ImageFilter.GaussianBlur(7 * SS))
    img.alpha_composite(blur)
    img.alpha_composite(glow)

    # category chips centred under the flag, kept clear of the footer
    cd = ImageDraw.Draw(img)
    f_chip = font(BOLD, 20 * SS)
    items = (("CIPHER", GREEN), ("RFID", CYAN), ("IR", AMBER))
    cyc = gy + int(scale * 0.30) if layout == "wide" else gy + int(scale * 0.36)
    chip_row(cd, gx, cyc, items, f_chip, 12 * SS)

    # text block
    tx = soft((w, h))
    td = ImageDraw.Draw(tx)
    if layout == "wide":
        x0, kick_y, title_y, title_px = 70 * SS, 84 * SS, 116 * SS, 128 * SS
    else:
        x0, kick_y, title_y, title_px = 70 * SS, 320 * SS, 352 * SS, 132 * SS

    f_kick = font(MONO, 22 * SS)
    f_title = font(BLACK_F, title_px)
    f_tag = font(BOLD, 36 * SS)
    f_sub = font(REG, 24 * SS)
    f_foot = font(MONO, 21 * SS)

    td.text((x0, kick_y), "FLIPPER ZERO  ·  ON-DEVICE CTF BOX", font=f_kick, fill=GREEN)
    td.text((x0 + 4 * SS, title_y + 4 * SS), "GAUNTLET", font=f_title,
            fill=(GREEN[0], GREEN[1], GREEN[2], 130))
    td.text((x0, title_y), "GAUNTLET", font=f_title, fill=WHITE)

    tag_y = title_y + title_px + 14 * SS
    td.text((x0, tag_y), "Run the gauntlet. Capture the flags.", font=f_tag, fill=AMBER)
    td.text((x0, tag_y + 48 * SS),
            "Load challenge packs and solve RFID, IR & cipher puzzles on the device.",
            font=f_sub, fill=GRAY)
    img.alpha_composite(tx)

    fd = ImageDraw.Draw(img)
    fd.line([(70 * SS, h - 54 * SS), (w - 70 * SS, h - 54 * SS)], fill=DIM, width=2 * SS)
    fd.text((70 * SS, h - 44 * SS), "github.com/at0m-b0mb/Gauntlet-FlipperZero",
            font=f_foot, fill=GRAY)
    fd.text((w - 70 * SS, h - 44 * SS), "MIT · by at0m-b0mb", font=f_foot, fill=GRAY, anchor="ra")

    out = img.convert("RGB").resize((W, H), Image.LANCZOS)
    out.save(path)
    print("wrote", path)


if __name__ == "__main__":
    render(os.path.join(OUT, "banner.png"), 1280, 400, layout="wide")
    render(os.path.join(OUT, "social-preview.png"), 1280, 640, layout="card")

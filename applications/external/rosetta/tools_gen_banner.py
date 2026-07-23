#!/usr/bin/env python3
"""Render the Rosetta GitHub banner + social-preview card.
Theme: "decode the wire". Deep slate background, a cyan brand glow, and the
app's own motif on the right - three signals (NFC field / OOK square wave /
1-Wire pulse) each being decoded into an annotated field chip. Supersampled."""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import os, math

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
BLACK_F = "/System/Library/Fonts/Supplemental/Arial Black.ttf"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
REG = "/System/Library/Fonts/Supplemental/Arial.ttf"

BG_TOP = (9, 13, 18)
BG_BOT = (15, 22, 30)
CYAN = (58, 224, 224)
GOLD = (255, 194, 75)
GREEN = (60, 220, 130)
WHITE = (236, 244, 246)
GRAY = (140, 156, 168)
DIM = (40, 52, 62)

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


def rrect(d, box, r, **kw):
    d.rounded_rectangle(box, radius=r, **kw)


def wave_nfc(d, x, y, w, col, lw):
    """A few concentric arcs - an RF field."""
    for i in range(3):
        r = int(w * (0.28 + i * 0.22))
        d.arc([x - r, y - r, x + r, y + r], 300, 420, fill=col, width=lw)


def wave_ook(d, x, y, w, col, lw):
    """An OOK burst envelope: on-off-on square groups."""
    seg = w / 8
    pts = []
    pattern = [1, 1, 0, 1, 0, 0, 1, 1]
    amp = int(w * 0.16)
    for i, b in enumerate(pattern):
        yy = y - amp if b else y + amp
        pts.append((x + i * seg, yy))
        pts.append((x + (i + 1) * seg, yy))
    d.line(pts, fill=col, width=lw, joint="curve")


def wave_1wire(d, x, y, w, col, lw):
    """A 1-Wire reset+presence pulse."""
    amp = int(w * 0.16)
    hi, lo = y - amp, y + amp
    pts = [
        (x, hi),
        (x + w * 0.08, hi),
        (x + w * 0.08, lo),
        (x + w * 0.42, lo),  # long reset low
        (x + w * 0.42, hi),
        (x + w * 0.52, hi),
        (x + w * 0.52, lo),
        (x + w * 0.74, lo),  # presence low
        (x + w * 0.74, hi),
        (x + w, hi),
    ]
    d.line(pts, fill=col, width=lw, joint="curve")


def chip_row(d, cx, y, chip_w, label, chip, accent):
    """A centered field chip + caption (used on the social card, no waveform)."""
    ch = 40 * SS
    rrect(
        d,
        [cx - chip_w // 2, y - ch // 2, cx + chip_w // 2, y + ch // 2],
        r=9 * SS,
        fill=(16, 24, 30),
        outline=accent,
        width=3 * SS,
    )
    f_chip = font(MONO, 22 * SS)
    bb = d.textbbox((0, 0), chip, font=f_chip)
    d.text(
        (cx - (bb[2] - bb[0]) / 2 - bb[0], y - (bb[3] - bb[1]) / 2 - bb[1]),
        chip,
        font=f_chip,
        fill=WHITE,
    )
    f_lab = font(MONO, 16 * SS)
    lb = d.textbbox((0, 0), label, font=f_lab)
    d.text(
        (cx - (lb[2] - lb[0]) / 2, y + ch // 2 + 7 * SS), label, font=f_lab, fill=GRAY
    )


def decode_row(d, x, y, w, drawer, label, chip, accent):
    """signal glyph --arrow--> field chip, with a caption."""
    sig_w = int(w * 0.30)
    drawer(d, x, y, sig_w, accent, 5 * SS)
    # arrow
    ax0 = x + int(sig_w * 0.75)
    ax1 = x + int(w * 0.46)
    d.line([(ax0, y), (ax1, y)], fill=GRAY, width=4 * SS)
    d.polygon([(ax1, y - 6 * SS), (ax1, y + 6 * SS), (ax1 + 10 * SS, y)], fill=GRAY)
    # chip
    cx0 = x + int(w * 0.50)
    ch = 34 * SS
    rrect(
        d,
        [cx0, y - ch // 2, x + w, y + ch // 2],
        r=8 * SS,
        fill=(16, 24, 30),
        outline=accent,
        width=3 * SS,
    )
    f_chip = font(MONO, 20 * SS)
    bb = d.textbbox((0, 0), chip, font=f_chip)
    d.text(
        (cx0 + (x + w - cx0 - (bb[2] - bb[0])) / 2, y - (bb[3] - bb[1]) / 2 - bb[1]),
        chip,
        font=f_chip,
        fill=WHITE,
    )
    f_lab = font(MONO, 15 * SS)
    d.text((x, y + ch // 2 + 6 * SS), label, font=f_lab, fill=GRAY)


def soft(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def render(path, W, H, layout="wide"):
    w, h = W * SS, H * SS
    img = vgradient(w, h).convert("RGBA")

    # brand glow behind the motif
    glow = soft((w, h))
    gd = ImageDraw.Draw(glow)
    if layout == "wide":
        gx, gy = int(w * 0.80), int(h * 0.5)
        rw = int(w * 0.26)
    else:
        gx, gy = int(w * 0.5), int(h * 0.66)
        rw = int(w * 0.42)
    gd.ellipse(
        [gx - rw, gy - rw, gx + rw, gy + rw], fill=(CYAN[0], CYAN[1], CYAN[2], 26)
    )
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(28 * SS)))

    # the three decode rows
    motif = soft((w, h))
    md = ImageDraw.Draw(motif)
    if layout == "wide":
        mx = int(w * 0.63)
        mw = int(w * 0.33)
        rows_y = [int(h * 0.28), int(h * 0.52), int(h * 0.76)]
        decode_row(
            md, mx, rows_y[0], mw, wave_nfc, "NFC  ANTICOLLISION", "SAK 08", CYAN
        )
        decode_row(md, mx, rows_y[1], mw, wave_ook, "SUB-GHZ  OOK/PSK", "1011", GOLD)
        decode_row(
            md, mx, rows_y[2], mw, wave_1wire, "1-WIRE  ROM + CRC", "CRC OK", GREEN
        )
    else:
        cx = int(w * 0.5)
        chip_w = int(w * 0.5)
        rows_y = [int(h * 0.48), int(h * 0.65), int(h * 0.82)]
        chip_row(
            md,
            cx,
            rows_y[0],
            chip_w,
            "NFC  ANTICOLLISION  ·  SAK / ATQA",
            "SAK 08",
            CYAN,
        )
        chip_row(
            md, cx, rows_y[1], chip_w, "SUB-GHZ  ·  OOK / PSK ENVELOPE", "1011", GOLD
        )
        chip_row(
            md, cx, rows_y[2], chip_w, "1-WIRE  ·  ROM + MAXIM CRC-8", "CRC OK", GREEN
        )
    img.alpha_composite(motif)

    # text block
    tx = soft((w, h))
    td = ImageDraw.Draw(tx)
    if layout == "wide":
        x0 = 70 * SS
        kick_y, title_y, title_px = 92 * SS, 126 * SS, 112 * SS
    else:
        x0 = 70 * SS
        kick_y, title_y, title_px = 40 * SS, 62 * SS, 112 * SS

    f_kick = font(MONO, 21 * SS)
    f_title = font(BLACK_F, title_px)
    f_tag = font(BOLD, 34 * SS)
    f_sub = font(REG, 22 * SS)
    f_foot = font(MONO, 21 * SS)

    td.text((x0, kick_y), "FLIPPER ZERO  ·  PROTOCOL EXPLAINER", font=f_kick, fill=CYAN)
    td.text(
        (x0 + 4 * SS, title_y + 4 * SS),
        "ROSETTA",
        font=f_title,
        fill=(CYAN[0], CYAN[1], CYAN[2], 130),
    )
    td.text((x0, title_y), "ROSETTA", font=f_title, fill=WHITE)

    tag_y = title_y + title_px + 16 * SS
    td.text((x0, tag_y), "Learn it, then watch it happen.", font=f_tag, fill=GOLD)
    td.text(
        (x0, tag_y + 46 * SS),
        "Mifare auth  ·  OOK/PSK  ·  1-Wire, live.",
        font=f_sub,
        fill=GRAY,
    )
    img.alpha_composite(tx)

    fd = ImageDraw.Draw(img)
    fd.line(
        [(70 * SS, h - 54 * SS), (w - 70 * SS, h - 54 * SS)], fill=DIM, width=2 * SS
    )
    fd.text(
        (70 * SS, h - 44 * SS),
        "github.com/at0m-b0mb/Rosetta-FlipperZero",
        font=f_foot,
        fill=GRAY,
    )
    fd.text(
        (w - 70 * SS, h - 44 * SS),
        "MIT · by at0m-b0mb",
        font=f_foot,
        fill=GRAY,
        anchor="ra",
    )

    out = img.convert("RGB").resize((W, H), Image.LANCZOS)
    out.save(path)
    print("wrote", path)


if __name__ == "__main__":
    render(os.path.join(OUT, "banner.png"), 1280, 400, layout="wide")
    render(os.path.join(OUT, "social-preview.png"), 1280, 640, layout="card")

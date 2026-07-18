#!/usr/bin/env python3
"""Render the Faraday GitHub banner + social-preview card.

The motif is the product in one picture: a key fob radiating inside a sealed
pouch, its carrier arcs blazing INSIDE the shield and dying to a faint whisper
outside it. Arcs are masked against the pouch shape so the containment is real
geometry, not a suggestion. Supersampled, then LANCZOS-downsampled.
"""
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops
import os

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
BLACK_F = "/System/Library/Fonts/Supplemental/Arial Black.ttf"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
REG = "/System/Library/Fonts/Supplemental/Arial.ttf"

# palette - cold shield, warm signal
BG_TOP = (8, 11, 16)
BG_BOT = (14, 20, 30)
SHIELD = (72, 214, 255)  # electric blue: the pouch
SIGNAL = (255, 168, 54)  # amber: the carrier being contained
GRAY = (148, 160, 176)
WHITE = (238, 245, 251)
DIM = (38, 50, 66)

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
        d.line(
            [(0, y), (w, y)],
            fill=tuple(int(BG_TOP[i] + (BG_BOT[i] - BG_TOP[i]) * t) for i in range(3)),
        )
    return img


def draw_pouch_path(d, x, y, w, h, r, **kw):
    """The pouch: a rounded body with a folded-over top flap."""
    d.rounded_rectangle([x, y, x + w, y + h], radius=r, **kw)


def build_arcs(size, cx, cy, n, base, step, lw):
    """Concentric carrier rings emanating from the fob."""
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for i in range(n):
        rr = base + i * step
        fade = int(255 * (1.0 - i / (n + 1)))
        d.ellipse(
            [cx - rr, cy - rr, cx + rr, cy + rr],
            outline=(SIGNAL[0], SIGNAL[1], SIGNAL[2], fade),
            width=lw,
        )
    return layer


def render(path, W, H, layout="wide"):
    w, h = W * SS, H * SS
    img = vgradient(w, h).convert("RGBA")

    # ---- geometry ----
    if layout == "wide":
        bx, by, bw, bh = int(w * 0.70), int(h * 0.20), int(w * 0.20), int(h * 0.60)
    else:
        bx, by, bw, bh = int(w * 0.38), int(h * 0.10), int(w * 0.24), int(h * 0.34)
    radius = int(bw * 0.16)
    cx, cy = bx + bw // 2, by + bh // 2

    # ---- carrier arcs, split by the pouch boundary ----
    arcs = build_arcs((w, h), cx, cy, n=9, base=int(bw * 0.22), step=int(bw * 0.20), lw=4 * SS)

    mask = Image.new("L", (w, h), 0)
    draw_pouch_path(ImageDraw.Draw(mask), bx, by, bw, bh, radius, fill=255)
    inv = ImageChops.invert(mask)

    alpha = arcs.getchannel("A")

    inside = arcs.copy()
    inside.putalpha(ImageChops.multiply(alpha, mask))

    # what escapes: the same rings at a whisper
    outside = arcs.copy()
    leak = ImageChops.multiply(alpha, inv).point(lambda a: int(a * 0.13))
    outside.putalpha(leak)

    img.alpha_composite(outside.filter(ImageFilter.GaussianBlur(3 * SS)))
    img.alpha_composite(outside)
    img.alpha_composite(inside.filter(ImageFilter.GaussianBlur(5 * SS)))
    img.alpha_composite(inside)

    # ---- the pouch itself ----
    shell = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shell)
    draw_pouch_path(sd, bx, by, bw, bh, radius, outline=SHIELD + (255,), width=5 * SS)
    # folded top flap + seam
    flap_h = int(bh * 0.16)
    sd.rounded_rectangle(
        [bx, by, bx + bw, by + flap_h], radius=int(radius * 0.6), outline=SHIELD + (255,), width=4 * SS
    )
    for i in range(1, 6):  # seal teeth
        tx = bx + int(bw * i / 6)
        sd.line([tx, by + int(flap_h * 0.25), tx, by + int(flap_h * 0.75)], fill=SHIELD + (200,), width=3 * SS)

    # the fob inside
    fw, fh = int(bw * 0.20), int(bw * 0.30)
    sd.rounded_rectangle(
        [cx - fw // 2, cy - fh // 2, cx + fw // 2, cy + fh // 2],
        radius=int(fw * 0.3),
        fill=SIGNAL + (255,),
    )
    sd.ellipse(
        [cx - fw // 6, cy - fh // 6, cx + fw // 6, cy + fh // 6], fill=(BG_TOP[0], BG_TOP[1], BG_TOP[2], 255)
    )

    img.alpha_composite(shell.filter(ImageFilter.GaussianBlur(6 * SS)))
    img.alpha_composite(shell)

    # ---- text ----
    tx = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    td = ImageDraw.Draw(tx)

    if layout == "wide":
        x0, kicker_y, title_y, title_px = 70 * SS, 96 * SS, 126 * SS, 120 * SS
    else:
        x0, kicker_y, title_y, title_px = 70 * SS, 320 * SS, 348 * SS, 126 * SS

    f_kick = font(MONO, 22 * SS)
    f_title = font(BLACK_F, title_px)
    f_tag = font(BOLD, 36 * SS)
    f_sub = font(REG, 23 * SS)
    f_foot = font(MONO, 21 * SS)

    td.text((x0, kicker_y), "FLIPPER ZERO  ·  SHIELDING TESTER", font=f_kick, fill=SHIELD)
    td.text((x0 + 4 * SS, title_y + 4 * SS), "FARADAY", font=f_title, fill=SIGNAL + (150,))
    td.text((x0, title_y), "FARADAY", font=f_title, fill=WHITE)

    tag_y = title_y + title_px + 16 * SS
    td.text((x0, tag_y), "Prove your signal-blocking pouch works.", font=f_tag, fill=SHIELD)
    td.text(
        (x0, tag_y + 48 * SS),
        "Measures real dB attenuation on Sub-GHz + NFC — no extra hardware.",
        font=f_sub,
        fill=GRAY,
    )

    img.alpha_composite(tx)

    fd = ImageDraw.Draw(img)
    fd.line([(70 * SS, h - 54 * SS), (w - 70 * SS, h - 54 * SS)], fill=DIM, width=2 * SS)
    fd.text(
        (70 * SS, h - 44 * SS),
        "github.com/at0m-b0mb/Faraday-FlipperZero",
        font=f_foot,
        fill=GRAY,
    )
    fd.text((w - 70 * SS, h - 44 * SS), "MIT · by at0m-b0mb", font=f_foot, fill=GRAY, anchor="ra")

    img.convert("RGB").resize((W, H), Image.LANCZOS).save(path)
    print("wrote", path)


if __name__ == "__main__":
    render(os.path.join(OUT, "banner.png"), 1280, 400, layout="wide")
    render(os.path.join(OUT, "social-preview.png"), 1280, 640, layout="card")

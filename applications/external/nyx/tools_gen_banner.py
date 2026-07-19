#!/usr/bin/env python3
"""Render the Nyx GitHub banner + social-preview card.
Night / near-IR theme: violet and IR-crimson on a near-black sky, with a
crescent moon, radiating IR beams, and a watching lens. Supersampled."""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import math, os

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
BLACK_F = "/System/Library/Fonts/Supplemental/Arial Black.ttf"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
REG = "/System/Library/Fonts/Supplemental/Arial.ttf"

# palette — the night and the invisible light in it
BG_TOP = (9, 7, 16)
BG_BOT = (20, 13, 34)
VIOLET = (159, 122, 255)
IRRED = (255, 74, 96)  # the 850/940 nm "glow" the camera leaks
GRAY = (150, 150, 172)
WHITE = (238, 236, 248)
DIM = (46, 40, 66)

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


def draw_crescent(d, cx, cy, r, col):
    """A crescent by subtracting an offset disc from a full one."""
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=col)
    off = int(r * 0.42)
    # punch the shadow using the background gradient's mid colour
    d.ellipse(
        [cx - r + off, cy - r - int(r * 0.12), cx + r + off, cy + r - int(r * 0.12)],
        fill=BG_BOT,
    )


def draw_beams(d, cx, cy, col, n=9, r0=40, r1=210, lw=4, spread=150, tilt=-38):
    """Radiating IR beams fanning out from the lens."""
    for i in range(n):
        a = math.radians(tilt - spread / 2 + spread * i / (n - 1))
        x0 = cx + math.cos(a) * r0
        y0 = cy + math.sin(a) * r0
        x1 = cx + math.cos(a) * r1
        y1 = cy + math.sin(a) * r1
        d.line([x0, y0, x1, y1], fill=col, width=lw)


def draw_lens(d, cx, cy, r, ring, iris):
    """A watching camera eye."""
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=ring, width=6 * SS)
    ir = int(r * 0.62)
    d.ellipse([cx - ir, cy - ir, cx + ir, cy + ir], fill=iris)
    pr = int(r * 0.26)
    d.ellipse([cx - pr, cy - pr, cx + pr, cy + pr], fill=BG_TOP)
    gr = max(2, int(r * 0.12))
    d.ellipse(
        [cx - ir + gr, cy - ir + gr, cx - ir + 4 * gr, cy - ir + 4 * gr], fill=WHITE
    )


def soft_layer(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def add_glow(base, layer, radius):
    base.alpha_composite(layer.filter(ImageFilter.GaussianBlur(radius)))
    base.alpha_composite(layer)


def render(path, W, H, layout="wide"):
    w, h = W * SS, H * SS
    img = vgradient(w, h).convert("RGBA")

    glow = soft_layer((w, h))
    gd = ImageDraw.Draw(glow)

    if layout == "wide":
        lx, ly, R = int(w * 0.82), int(h * 0.52), int(h * 0.20)
        mx, my, mr = int(w * 0.9), int(h * 0.24), int(h * 0.12)
    else:
        lx, ly, R = int(w * 0.5), int(h * 0.32), int(h * 0.13)
        mx, my, mr = int(w * 0.66), int(h * 0.14), int(h * 0.08)

    # IR beams behind the lens, faint crimson
    draw_beams(
        gd,
        lx,
        ly,
        (IRRED[0], IRRED[1], IRRED[2], 90),
        n=9,
        r0=int(R * 0.9),
        r1=int(R * 5.2),
        lw=5 * SS,
        spread=150,
        tilt=-140,
    )
    # crescent moon up and to the side
    draw_crescent(gd, mx, my, mr, (VIOLET[0], VIOLET[1], VIOLET[2], 235))
    # the watching lens
    draw_lens(
        gd,
        lx,
        ly,
        R,
        ring=(VIOLET[0], VIOLET[1], VIOLET[2], 255),
        iris=(IRRED[0], IRRED[1], IRRED[2], 255),
    )
    add_glow(img, glow, radius=9 * SS)

    # ---- text ----
    tx = soft_layer((w, h))
    td = ImageDraw.Draw(tx)

    if layout == "wide":
        x0, kicker_y, title_y, title_px = 70 * SS, 88 * SS, 116 * SS, 132 * SS
    else:
        x0, kicker_y, title_y, title_px = 70 * SS, 320 * SS, 346 * SS, 132 * SS

    f_kick = font(MONO, 22 * SS)
    f_title = font(BLACK_F, title_px)
    f_tag = font(BOLD, 37 * SS)
    f_sub = font(REG, 24 * SS)
    f_foot = font(MONO, 21 * SS)

    td.text(
        (x0, kicker_y), "FLIPPER ZERO  ·  IR CAMERA SWEEP", font=f_kick, fill=VIOLET
    )

    # title with an IR-crimson offset, like a heat ghost
    td.text(
        (x0 + 4 * SS, title_y + 4 * SS),
        "NYX",
        font=f_title,
        fill=(IRRED[0], IRRED[1], IRRED[2], 150),
    )
    td.text((x0, title_y), "NYX", font=f_title, fill=WHITE)

    tag_y = title_y + title_px + 14 * SS
    td.text(
        (x0, tag_y), "See the light they hoped you couldn't.", font=f_tag, fill=VIOLET
    )
    td.text(
        (x0, tag_y + 50 * SS),
        "Counter-surveillance IR sweep for covert night-vision cameras.",
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
        "github.com/at0m-b0mb/Nyx-FlipperZero",
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

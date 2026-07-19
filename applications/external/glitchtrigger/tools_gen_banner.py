#!/usr/bin/env python3
"""Render the Glitch Trigger GitHub banner + social-preview card.
Theme: "drop the rail, catch the fault". Deep slate background, an amber brand
glow, and the app's own motif - a power-rail scope trace that a sharp glitch
notch pulls low, annotated with the trigger -> delay -> glitch timeline.
Supersampled for crisp edges."""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import os

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
BLACK_F = "/System/Library/Fonts/Supplemental/Arial Black.ttf"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
REG = "/System/Library/Fonts/Supplemental/Arial.ttf"

BG_TOP = (10, 12, 16)
BG_BOT = (18, 15, 19)
AMBER = (255, 178, 40)
RED = (255, 82, 68)
STEEL = (96, 124, 148)
WHITE = (238, 241, 245)
GRAY = (144, 154, 166)
DIM = (44, 48, 56)

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


def soft(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def dashed_h(d, x0, x1, y, col, lw, dash):
    x = x0
    while x < x1:
        d.line([(x, y), (min(x + dash, x1), y)], fill=col, width=lw)
        x += dash * 2


def caret(d, cx, y, col, s):
    """small up-caret marking a point on the trace."""
    d.polygon([(cx - s, y + s), (cx + s, y + s), (cx, y)], fill=col)


def draw_scope(md, x, y, w, h):
    """The hero motif: rail high, a glitch notch pulling it low, annotated.
    (x,y) is the top-left of the drawing area; w,h its size."""
    yhi = y + int(h * 0.30)  # rail (idle) level - HIGH
    ylo = y + int(h * 0.72)  # glitch floor - LOW
    x0 = x
    x1 = x + w

    # faint baseline grid
    for gy in (yhi, ylo):
        dashed_h(md, x0, x1, gy, (54, 60, 70, 255), 2 * SS, 10 * SS)

    xtrig = x0 + int(w * 0.16)
    xg0 = x0 + int(w * 0.58)  # glitch start
    xgw = int(w * 0.05)  # glitch width (drawn)
    xg1 = xg0 + xgw

    lw = 6 * SS
    # rail trace: high -> (delay) -> notch down -> recover -> high
    md.line([(x0, yhi), (xg0, yhi)], fill=AMBER, width=lw)  # rail up to glitch
    md.line([(xg0, yhi), (xg0, ylo)], fill=RED, width=lw)  # falling edge
    md.line([(xg0, ylo), (xg1, ylo)], fill=RED, width=lw)  # glitch floor
    md.line([(xg1, ylo), (xg1, yhi)], fill=RED, width=lw)  # rising edge
    md.line([(xg1, yhi), (x1, yhi)], fill=AMBER, width=lw)  # rail recovers

    # trigger marker
    md.line([(xtrig, yhi - int(h * 0.16)), (xtrig, ylo + int(h * 0.14))],
            fill=STEEL, width=3 * SS)
    caret(md, xtrig, yhi - int(h * 0.16), STEEL, 6 * SS)

    # delay bracket (dashed, trigger -> glitch)
    ymark = yhi - int(h * 0.10)
    dashed_h(md, xtrig, xg0, ymark, (STEEL[0], STEEL[1], STEEL[2], 255), 3 * SS, 8 * SS)

    # glitch glow
    glow = soft(md._image.size if hasattr(md, "_image") else (1, 1))

    # labels
    f_lab = font(MONO, 17 * SS)
    f_small = font(MONO, 15 * SS)
    md.text((xtrig, ylo + int(h * 0.16)), "TRIGGER", font=f_small, fill=STEEL, anchor="ma")
    mid = (xtrig + xg0) // 2
    md.text((mid, ymark - 22 * SS), "DELAY", font=f_small, fill=STEEL, anchor="ma")
    md.text(((xg0 + xg1) // 2, ylo + int(h * 0.06)), "GLITCH", font=f_lab, fill=RED, anchor="ma")


def chips(md, x, y, items, accent):
    """A row of small parameter chips."""
    f = font(MONO, 18 * SS)
    cx = x
    ch = 34 * SS
    for label in items:
        bb = md.textbbox((0, 0), label, font=f)
        cw = (bb[2] - bb[0]) + 20 * SS
        rrect(md, [cx, y, cx + cw, y + ch], r=8 * SS, fill=(20, 24, 30),
              outline=accent, width=2 * SS)
        md.text((cx + 10 * SS, y + ch // 2), label, font=f, fill=WHITE, anchor="lm")
        cx += cw + 12 * SS


def render(path, W, H, layout="wide"):
    w, h = W * SS, H * SS
    img = vgradient(w, h).convert("RGBA")

    # amber brand glow behind the motif
    glow = soft((w, h))
    gd = ImageDraw.Draw(glow)
    if layout == "wide":
        gx, gy, rw = int(w * 0.74), int(h * 0.46), int(w * 0.24)
    else:
        gx, gy, rw = int(w * 0.5), int(h * 0.60), int(w * 0.40)
    gd.ellipse([gx - rw, gy - rw, gx + rw, gy + rw], fill=(AMBER[0], AMBER[1], AMBER[2], 30))
    # a hot red core
    rr = rw // 2
    gd.ellipse([gx - rr, gy - rr, gx + rr, gy + rr], fill=(RED[0], RED[1], RED[2], 22))
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(30 * SS)))

    motif = soft((w, h))
    md = ImageDraw.Draw(motif)
    if layout == "wide":
        draw_scope(md, int(w * 0.55), int(h * 0.15), int(w * 0.40), int(h * 0.48))
        chips(md, int(w * 0.55), int(h * 0.75),
              ["DELAY 100us", "WIDTH 500ns", "x1"], AMBER)
    else:
        draw_scope(md, int(w * 0.14), int(h * 0.52), int(w * 0.72), int(h * 0.30))
        chips(md, int(w * 0.34), int(h * 0.85),
              ["MANUAL", "EXTERNAL", "SWEEP"], AMBER)
    img.alpha_composite(motif)

    tx = soft((w, h))
    td = ImageDraw.Draw(tx)
    x0 = 70 * SS
    f_kick = font(MONO, 20 * SS)
    f_tag = font(BOLD, 30 * SS)
    f_sub = font(REG, 20 * SS)
    f_foot = font(MONO, 20 * SS)

    if layout == "wide":
        kick_y = 52 * SS
        title_px = 74 * SS
        line1_y = 74 * SS
        line_gap = int(title_px * 0.98)
        line2_y = line1_y + line_gap
        f_title = font(BLACK_F, title_px)

        td.text((x0, kick_y), "FLIPPER ZERO  ·  FAULT INJECTION", font=f_kick, fill=AMBER)
        td.text((x0 + 3 * SS, line1_y + 3 * SS), "GLITCH", font=f_title,
                fill=(RED[0], RED[1], RED[2], 120))
        td.text((x0, line1_y), "GLITCH", font=f_title, fill=WHITE)
        td.text((x0, line2_y), "TRIGGER", font=f_title, fill=AMBER)

        tag_y = line2_y + title_px + 20 * SS
        td.text((x0, tag_y), "Drop the rail. Catch the fault.", font=f_tag, fill=WHITE)
        td.text((x0, tag_y + 40 * SS),
                "Cycle-accurate GPIO glitch pulses  ·  manual / external / sweep.",
                font=f_sub, fill=GRAY)
    else:
        kick_y = 54 * SS
        title_px = 92 * SS
        line1_y = 80 * SS
        line_gap = int(title_px * 0.98)
        line2_y = line1_y + line_gap
        f_title = font(BLACK_F, title_px)

        td.text((x0, kick_y), "FLIPPER ZERO  ·  FAULT INJECTION", font=f_kick, fill=AMBER)
        td.text((x0 + 3 * SS, line1_y + 3 * SS), "GLITCH", font=f_title,
                fill=(RED[0], RED[1], RED[2], 120))
        td.text((x0, line1_y), "GLITCH", font=f_title, fill=WHITE)
        td.text((x0, line2_y), "TRIGGER", font=f_title, fill=AMBER)

        tag_y = line2_y + title_px + 20 * SS
        td.text((x0, tag_y), "Drop the rail. Catch the fault.", font=f_tag, fill=WHITE)
    img.alpha_composite(tx)

    fd = ImageDraw.Draw(img)
    fd.line([(70 * SS, h - 54 * SS), (w - 70 * SS, h - 54 * SS)], fill=DIM, width=2 * SS)
    fd.text((70 * SS, h - 44 * SS),
            "github.com/at0m-b0mb/GlitchTrigger-FlipperZero", font=f_foot, fill=GRAY)
    fd.text((w - 70 * SS, h - 44 * SS), "MIT · by at0m-b0mb", font=f_foot, fill=GRAY, anchor="ra")

    out = img.convert("RGB").resize((W, H), Image.LANCZOS)
    out.save(path)
    print("wrote", path)


if __name__ == "__main__":
    render(os.path.join(OUT, "banner.png"), 1280, 400, layout="wide")
    render(os.path.join(OUT, "social-preview.png"), 1280, 640, layout="card")

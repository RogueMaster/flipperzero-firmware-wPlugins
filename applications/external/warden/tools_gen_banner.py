#!/usr/bin/env python3
"""Render the Warden GitHub banner + social-preview card.
Authority/verdict theme: deep navy, a violet brand glow, a big letter-grade
stamp and a green->red risk meter (the app's own motif). Supersampled."""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import os

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
BLACK_F = "/System/Library/Fonts/Supplemental/Arial Black.ttf"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
REG = "/System/Library/Fonts/Supplemental/Arial.ttf"

BG_TOP = (10, 12, 20)
BG_BOT = (17, 20, 36)
VIOLET = (168, 120, 255)
GREEN = (60, 220, 130)
AMBER = (255, 190, 60)
RED = (255, 74, 74)
WHITE = (238, 242, 250)
GRAY = (150, 160, 182)
DIM = (46, 52, 74)

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


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def spectrum(t):
    """green -> amber -> red across t in [0,1]."""
    if t < 0.5:
        return lerp(GREEN, AMBER, t / 0.5)
    return lerp(AMBER, RED, (t - 0.5) / 0.5)


def rrect(d, box, r, **kw):
    d.rounded_rectangle(box, radius=r, **kw)


def draw_grade_stamp(d, cx, cy, s, letter, ring=VIOLET):
    """A rounded-square grade badge with a big letter."""
    half = s // 2
    rrect(d, [cx - half, cy - half, cx + half, cy + half], r=s // 6,
          fill=(14, 16, 28), outline=ring, width=6 * SS)
    # corner ticks
    for dx, dy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
        px = cx + dx * (half - 14 * SS)
        py = cy + dy * (half - 14 * SS)
        d.ellipse([px - 3 * SS, py - 3 * SS, px + 3 * SS, py + 3 * SS], fill=ring)
    f = font(BLACK_F, int(s * 0.62))
    bb = d.textbbox((0, 0), letter, font=f)
    tw, th = bb[2] - bb[0], bb[3] - bb[1]
    d.text((cx - tw / 2 - bb[0], cy - th / 2 - bb[1]), letter, font=f, fill=WHITE)


def draw_meter(d, x, y, w, h, segs=12, marker=0.16):
    """Segmented green->red risk meter with a marker triangle."""
    gap = int(w * 0.012)
    sw = (w - gap * (segs - 1)) / segs
    for i in range(segs):
        t = i / (segs - 1)
        col = spectrum(t)
        sx = x + i * (sw + gap)
        rrect(d, [sx, y, sx + sw, y + h], r=int(h * 0.28), fill=col)
    # marker
    mx = int(x + marker * w)
    d.polygon([(mx - 9 * SS, y - 16 * SS), (mx + 9 * SS, y - 16 * SS), (mx, y - 3 * SS)],
              fill=WHITE)


def draw_shield(d, cx, cy, w, col, lw):
    h = int(w * 1.18)
    top = cy - h // 2
    pts = [
        (cx - w // 2, top),
        (cx + w // 2, top),
        (cx + w // 2, top + int(h * 0.42)),
        (cx, cy + h // 2),
        (cx - w // 2, top + int(h * 0.42)),
    ]
    d.line(pts + [pts[0]], fill=col, width=lw, joint="curve")
    # keyhole
    kr = w // 9
    d.ellipse([cx - kr, cy - kr - h // 12, cx + kr, cy + kr - h // 12], outline=col, width=lw)
    d.line([cx, cy - h // 12, cx, cy + h // 6], fill=col, width=lw)


def field_arcs(d, cx, cy, col, n=4, base=60, step=44, lw=4, a=(200, 340)):
    for i in range(n):
        r = base + i * step
        d.arc([cx - r, cy - r, cx + r, cy + r], a[0], a[1], fill=col, width=lw)


def soft(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def render(path, W, H, layout="wide"):
    w, h = W * SS, H * SS
    img = vgradient(w, h).convert("RGBA")

    glow = soft((w, h))
    gd = ImageDraw.Draw(glow)
    if layout == "wide":
        gx, gy = int(w * 0.80), int(h * 0.42)
        stamp = int(h * 0.46)
    else:
        gx, gy = int(w * 0.5), int(h * 0.30)
        stamp = int(h * 0.22)

    field_arcs(gd, gx, gy, (VIOLET[0], VIOLET[1], VIOLET[2], 60),
               n=4, base=int(stamp * 0.7), step=int(stamp * 0.5), lw=5 * SS)
    draw_shield(gd, int(gx - stamp * 1.15), gy, int(stamp * 0.7),
                (VIOLET[0], VIOLET[1], VIOLET[2], 130), 4 * SS)
    draw_grade_stamp(gd, gx, gy, stamp, "A+")
    draw_meter(gd, int(gx - stamp * 0.62), int(gy + stamp * 0.72),
               int(stamp * 1.25), 12 * SS, segs=12, marker=0.14)

    blur = glow.filter(ImageFilter.GaussianBlur(9 * SS))
    img.alpha_composite(blur)
    img.alpha_composite(glow)

    tx = soft((w, h))
    td = ImageDraw.Draw(tx)
    if layout == "wide":
        x0 = 70 * SS
        kick_y, title_y, title_px = 78 * SS, 108 * SS, 132 * SS
    else:
        x0 = 70 * SS
        kick_y, title_y, title_px = 300 * SS, 328 * SS, 132 * SS

    f_kick = font(MONO, 22 * SS)
    f_title = font(BLACK_F, title_px)
    f_tag = font(BOLD, 37 * SS)
    f_sub = font(REG, 24 * SS)
    f_foot = font(MONO, 21 * SS)

    td.text((x0, kick_y), "FLIPPER ZERO  ·  NFC ACCESS-CARD GRADER", font=f_kick, fill=VIOLET)
    td.text((x0 + 4 * SS, title_y + 4 * SS), "WARDEN", font=f_title, fill=(VIOLET[0], VIOLET[1], VIOLET[2], 150))
    td.text((x0, title_y), "WARDEN", font=f_title, fill=WHITE)

    tag_y = title_y + title_px + 12 * SS
    td.text((x0, tag_y), "Grade your access badges. See what a cloner sees.", font=f_tag, fill=GREEN)
    td.text((x0, tag_y + 48 * SS),
            "Hold a badge to the Flipper — get an A+ to F security grade in plain English.",
            font=f_sub, fill=GRAY)
    img.alpha_composite(tx)

    fd = ImageDraw.Draw(img)
    fd.line([(70 * SS, h - 54 * SS), (w - 70 * SS, h - 54 * SS)], fill=DIM, width=2 * SS)
    fd.text((70 * SS, h - 44 * SS), "github.com/at0m-b0mb/Warden-FlipperZero", font=f_foot, fill=GRAY)
    fd.text((w - 70 * SS, h - 44 * SS), "MIT · by at0m-b0mb", font=f_foot, fill=GRAY, anchor="ra")

    out = img.convert("RGB").resize((W, H), Image.LANCZOS)
    out.save(path)
    print("wrote", path)


if __name__ == "__main__":
    render(os.path.join(OUT, "banner.png"), 1280, 400, layout="wide")
    render(os.path.join(OUT, "social-preview.png"), 1280, 640, layout="card")

#!/usr/bin/env python3
"""Render the Specter GitHub banner + social-preview card.
Dark spectral theme (cyan/magenta on near-black) with the EMF gauge motif,
a little specter, and radiating reader-field arcs. Supersampled for smoothness."""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import math, os

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
BLACK_F = "/System/Library/Fonts/Supplemental/Arial Black.ttf"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
REG = "/System/Library/Fonts/Supplemental/Arial.ttf"

# palette
BG_TOP = (7, 10, 15)
BG_BOT = (12, 18, 30)
CYAN = (38, 232, 202)
MAG = (255, 61, 174)
GRAY = (150, 162, 178)
WHITE = (236, 244, 250)
DIM = (40, 52, 68)

SS = 2  # supersample


def font(path, px):
    try:
        return ImageFont.truetype(path, px)
    except OSError:
        return ImageFont.truetype(BOLD, px)


def vgradient(w, h):
    img = Image.new("RGB", (w, h), BG_TOP)
    top, bot = BG_TOP, BG_BOT
    for y in range(h):
        t = y / max(1, h - 1)
        r = int(top[0] + (bot[0] - top[0]) * t)
        g = int(top[1] + (bot[1] - top[1]) * t)
        b = int(top[2] + (bot[2] - top[2]) * t)
        for x in range(0, w, w):
            pass
        ImageDraw.Draw(img).line([(0, y), (w, y)], fill=(r, g, b))
    return img


def gauge_point(cx, cy, value, radius):
    value = max(0, min(100, value))
    a = math.pi * (1.0 - value / 100.0)
    return cx + math.cos(a) * radius, cy - math.sin(a) * radius


def draw_gauge(d, cx, cy, R, strength, col=CYAN, lw=6):
    # arc
    pts = []
    v = 0
    while v <= 100:
        pts.append(gauge_point(cx, cy, v, R))
        v += 2
    d.line(pts, fill=col, width=lw, joint="curve")
    # ticks
    for i in range(11):
        vv = i * 10
        hot = i >= 8
        ox, oy = gauge_point(cx, cy, vv, R)
        ix, iy = gauge_point(cx, cy, vv, R - (26 if hot else 16))
        d.line([ix, iy, ox, oy], fill=(MAG if hot else col), width=lw if hot else lw - 2)
    # needle
    tx, ty = gauge_point(cx, cy, strength, R - 18)
    d.line([cx, cy, tx, ty], fill=WHITE, width=lw + 1)
    d.ellipse([tx - 9, ty - 9, tx + 9, ty + 9], fill=MAG)
    # hub
    d.ellipse([cx - 16, cy - 16, cx + 16, cy + 16], fill=col)
    d.ellipse([cx - 7, cy - 7, cx + 7, cy + 7], fill=BG_BOT)


def draw_specter(d, cx, cy, w, col=CYAN, eye=BG_TOP):
    """A little ghost centred at (cx, cy)."""
    h = int(w * 1.15)
    left = cx - w // 2
    right = cx + w // 2
    top = cy - h // 2
    bot = cy + h // 2
    # dome
    d.pieslice([left, top, right, top + w], 180, 360, fill=col)
    # body
    d.rectangle([left, top + w // 2, right, bot - w // 6], fill=col)
    # wavy feet
    n = 4
    fw = (right - left) / n
    for i in range(n):
        x0 = left + i * fw
        d.polygon(
            [(x0, bot - w // 6), (x0 + fw / 2, bot - w // 6),
             (x0 + fw / 4, bot)],
            fill=BG_TOP,
        )
    # eyes
    er = w // 7
    ey = top + w // 2 + er
    for ex in (cx - w // 4, cx + w // 4):
        d.ellipse([ex - er, ey - er, ex + er, ey + er + er // 2], fill=eye)
        d.ellipse([ex - er // 2, ey, ex + er // 3, ey + er + er // 2], fill=MAG)


def draw_field_arcs(d, cx, cy, col, n=5, base=70, step=46, lw=4, start=200, end=340):
    for i in range(n):
        r = base + i * step
        d.arc([cx - r, cy - r, cx + r, cy + r], start, end, fill=col, width=lw)


def soft_layer(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def add_glow(base, layer, radius, alpha=255):
    blur = layer.filter(ImageFilter.GaussianBlur(radius))
    base.alpha_composite(blur)
    base.alpha_composite(layer)


def render(path, W, H, layout="wide"):
    w, h = W * SS, H * SS
    img = vgradient(w, h).convert("RGBA")

    # The text block is authored against a 1280x400 canvas. Every offset and
    # font size below is scaled by K so a 2x asset is the same design at twice
    # the resolution, rather than the same absolute pixels marooned in the
    # corner of a bigger picture.
    K = W / 1280.0

    def u(v):
        return int(round(v * SS * K))

    # ---- right-side glow motif: field arcs + gauge + specter ----
    glow = soft_layer((w, h))
    gd = ImageDraw.Draw(glow)

    if layout == "wide":
        gx, gy, R = int(w * 0.80), int(h * 0.46), int(h * 0.32)
    else:
        gx, gy, R = int(w * 0.5), int(h * 0.26), int(h * 0.19)

    # radiating reader-field arcs (behind), faint
    draw_field_arcs(gd, gx, int(gy - R * 0.1), (CYAN[0], CYAN[1], CYAN[2], 70),
                    n=5, base=int(R * 0.7), step=int(R * 0.42), lw=5 * SS,
                    start=205, end=335)
    draw_gauge(gd, gx, gy, R, 82, col=CYAN, lw=5 * SS)
    draw_specter(gd, gx, int(gy - R * 0.45), int(R * 0.5), col=CYAN)
    add_glow(img, glow, radius=10 * SS)

    # ---- text block ----
    tx = soft_layer((w, h))
    td = ImageDraw.Draw(tx)

    if layout == "wide":
        x0 = u(70)
        kicker_y = u(72)
        title_y = u(100)
        title_px = u(130)
    else:
        x0 = u(70)
        kicker_y = u(300)
        title_y = u(326)
        title_px = u(132)

    f_kick = font(MONO, u(22))
    f_title = font(BLACK_F, title_px)
    f_tag = font(BOLD, u(38))
    f_sub = font(REG, u(24))
    f_foot = font(MONO, u(21))

    # kicker
    td.text((x0, kicker_y), "FLIPPER ZERO  ·  NFC BUG SWEEP",
            font=f_kick, fill=CYAN)

    # title with magenta offset shadow
    td.text((x0 + u(4), title_y + u(4)), "SPECTER", font=f_title, fill=(MAG[0], MAG[1], MAG[2], 160))
    td.text((x0, title_y), "SPECTER", font=f_title, fill=WHITE)

    # version pill, sitting on the title's baseline
    f_ver = font(BOLD, u(26))
    title_w = td.textlength("SPECTER", font=f_title)
    px, py = x0 + title_w + u(22), title_y + title_px - u(46)
    pw, ph = u(84), u(38)
    td.rounded_rectangle([px, py, px + pw, py + ph], radius=u(10), fill=MAG)
    td.text((px + pw / 2, py + ph / 2), "v2.8", font=f_ver, fill=BG_TOP, anchor="mm")

    # tagline + subtitle
    tag_y = title_y + title_px + u(14)
    td.text((x0, tag_y), "Sweep for the readers you can't see.", font=f_tag, fill=CYAN)
    td.text((x0, tag_y + u(50)),
            "Find it · fingerprint it · survey the room · leave it on watch. No extra hardware.",
            font=f_sub, fill=GRAY)

    img.alpha_composite(tx)

    # footer line + url
    fd = ImageDraw.Draw(img)
    fd.line([(u(70), h - u(54)), (w - u(70), h - u(54))], fill=DIM, width=max(2, u(2)))
    fd.text((u(70), h - u(44)), "github.com/at0m-b0mb/Specter-FlipperZero",
            font=f_foot, fill=GRAY)
    fd.text((w - u(70), h - u(44)), "MIT · by at0m-b0mb",
            font=f_foot, fill=GRAY, anchor="ra")

    out = img.convert("RGB").resize((W, H), Image.LANCZOS)
    out.save(path)
    print("wrote", path)


def render_mark(path, size=512):
    """Square logo mark: the gauge and the specter, no wordmark.

    Used anywhere the project needs an icon rather than a banner - a GitHub
    org avatar, a social profile, a favicon."""
    w = size * SS
    img = vgradient(w, w).convert("RGBA")

    glow = soft_layer((w, w))
    gd = ImageDraw.Draw(glow)
    # Pivot low so the arcs above it and the hub below balance in the square.
    cx, cy, R = w // 2, int(w * 0.68), int(w * 0.30)

    draw_field_arcs(gd, cx, int(cy - R * 0.15), (CYAN[0], CYAN[1], CYAN[2], 80),
                    n=3, base=int(R * 1.25), step=int(R * 0.30), lw=4 * SS,
                    start=205, end=335)
    draw_gauge(gd, cx, cy, R, 82, col=CYAN, lw=5 * SS)
    draw_specter(gd, cx, int(cy - R * 0.45), int(R * 0.52), col=CYAN)
    add_glow(img, glow, radius=8 * SS)

    out = img.convert("RGB").resize((size, size), Image.LANCZOS)
    out.save(path)
    print("wrote", path, f"({size}x{size})")


if __name__ == "__main__":
    # README banner at 2x: GitHub's content column is ~1000px, so a 2560px
    # asset displayed at width=100% stays crisp on a retina screen.
    render(os.path.join(OUT, "banner.png"), 2560, 800, layout="wide")

    # GitHub social preview. 1280x640 is the size GitHub recommends for repo
    # cards (minimum 640x320, 1 MB limit) - do not change this one.
    render(os.path.join(OUT, "social-preview.png"), 1280, 640, layout="card")

    render_mark(os.path.join(OUT, "mark.png"), 512)

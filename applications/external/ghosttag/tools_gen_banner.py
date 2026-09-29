#!/usr/bin/env python3
"""Render GhostTag's brand assets: banner (light + dark), social card, and mark.

DWELL
-----
GhostTag is not about detecting a tracker. Detecting one is easy and almost
always innocent - there are tags in every bag on the train. It is about
PERSISTENCE: the thing that is still with you twenty minutes later. So the
brand's one measured object is a dwell plot, and the accent marks exactly one
thing on it.

Two laws, both enforced here rather than merely intended:

  LAW 1, THE ORANGE QUARANTINE. EMBER (#FE8A2C) is never a stroke this file
  draws. It is not a chosen colour - it is literally the value the device
  emits, because every capture in screenshots/ is exactly two colours,
  (254,138,44) and (0,0,0). The only ember on any asset is light the Flipper
  actually emitted, i.e. pixels inside a real capture, pasted untouched.
  _guard() raises if EMBER reaches a draw call.

  LAW 2, THE ACCENT MEANS "THIS ONE IS FOLLOWING YOU". The gold ramp draws the
  promoted tracker's trace and its trip marker, and nothing else - never a
  rule, a frame, a mount or a word. Everything else in the room is drawn in
  the neutral ramp, which is the entire argument the product makes, in one
  picture. text() raises if an accent token is passed as a text fill.

Nothing here is typed that could be read instead: the version comes from
ghosttag_i.h, and the traces, the trip moment and the dwell window all come
from helpers/demo_source.c via tools_brand_data.py - computed with the same
arithmetic the firmware runs on the device.

    python3 tools_gen_banner.py
"""
import os

from PIL import Image, ImageChops, ImageDraw, ImageFont

from tools_brand_data import (
    alert_moment,
    demo_cast,
    demo_follow_ms,
    demo_loop_ms,
    min_detections,
    rssi_series,
    version,
)

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "images")
SHOTS = os.path.join(HERE, "screenshots")
os.makedirs(OUT, exist_ok=True)

# --- palette ---------------------------------------------------------------
PAPER = (243, 241, 236)  # #F3F1EC  light ground, flat, never a gradient
BLACK = (0, 0, 0)  # #000000  true black - explicitly not navy
INK = (22, 19, 13)  # #16130D  identity type on paper
GRAPHITE = (87, 82, 74)  # #57524A  secondary type on paper
HAIRLINE = (217, 212, 200)  # #D9D4C8  rules and mounts on paper - structure only
BONE = (243, 241, 236)  # #F3F1EC  identity type on black
ASH = (169, 162, 154)  # #A9A29A  secondary type on black
SCORE = (42, 39, 36)  # #2A2724  rules and mounts on black - structure only

EMBER = (254, 138, 44)  # QUARANTINED. Sampled from the device. Never drawn.
AMBER = (216, 134, 42)  # #D8862A  the follower, on black
BRASS_DEEP = (138, 103, 20)  # #8A6714  the follower, on paper

RAMP = {EMBER, AMBER, BRASS_DEEP}

THEME = {
    "light": dict(
        ground=PAPER,
        ink=INK,
        second=GRAPHITE,
        rule=HAIRLINE,
        quiet=(198, 192, 180),
        accent=BRASS_DEEP,
        suffix="",
    ),
    "dark": dict(
        ground=BLACK,
        ink=BONE,
        second=ASH,
        rule=SCORE,
        quiet=(72, 68, 63),
        accent=AMBER,
        suffix="-dark",
    ),
}

# --- type ------------------------------------------------------------------
# Didot because instrument nameplates were engraved in Didone. Andale Mono for
# measured values, because it is the same shape family the device's own
# FontSecondary stands in as.
SERIF = "/System/Library/Fonts/Supplemental/Didot.ttc"  # 0 Regular, 2 Bold
SANS = "/System/Library/Fonts/Supplemental/Futura.ttc"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"


def font(path, px, index=0):
    try:
        return ImageFont.truetype(path, px, index=index)
    except OSError:
        return ImageFont.load_default()


class SafeBox:
    """Every rectangle that carries MEANING, so the margin is asserted rather
    than eyeballed.

    Background art is never registered, which is what lets it bleed off the
    edge without tripping the check. Pixel-measuring the finished PNG cannot
    make that distinction - a ring that is supposed to run off the canvas reads
    as an overflow - so the registration happens as things are drawn.
    """

    def __init__(self, w, h, margin):
        self.w, self.h, self.margin = w, h, margin
        self.boxes = []

    def add(self, x0, y0, x1, y1, what):
        self.boxes.append((x0, y0, x1, y1, what))

    def no_overlap(self, *, ignore=()):
        """Registered elements must not run into each other.

        The border check cannot see this: two things can both sit comfortably
        inside the safe box and still be printed on top of one another. The
        wordmark's last two letters ran straight through the plot's label the
        first time this banner rendered, and nothing caught it but an eyeball.
        """
        for i in range(len(self.boxes)):
            for j in range(i + 1, len(self.boxes)):
                ax0, ay0, ax1, ay1, aw = self.boxes[i]
                bx0, by0, bx1, by1, bw = self.boxes[j]
                if (aw, bw) in ignore or (bw, aw) in ignore:
                    continue
                if ax0 < bx1 and bx0 < ax1 and ay0 < by1 and by0 < ay1:
                    raise AssertionError(
                        f"{aw} overlaps {bw}: "
                        f"{tuple(int(v) for v in (ax0, ay0, ax1, ay1))} vs "
                        f"{tuple(int(v) for v in (bx0, by0, bx1, by1))}"
                    )

    def check(self):
        if not self.boxes:
            return None
        worst = None
        for x0, y0, x1, y1, what in self.boxes:
            m = min(x0, y0, self.w - x1, self.h - y1)
            if worst is None or m < worst[0]:
                worst = (m, what, (x0, y0, x1, y1))
        m, what, box = worst
        if m < self.margin:
            raise AssertionError(
                f"{what} at {tuple(int(v) for v in box)} leaves a {int(m)}px margin on a "
                f"{self.w}x{self.h} canvas; the safe border needs {self.margin}px. "
                f"Move it inside x {self.margin}..{self.w - self.margin}, "
                f"y {self.margin}..{self.h - self.margin}."
            )
        return m, what


def _guard(fill):
    if tuple(fill) == EMBER:
        raise AssertionError(
            "EMBER is quarantined: it is the value the device emits, so it may only "
            "appear inside a real capture pasted untouched, never as a drawn stroke."
        )
    return fill


def text(d, x, y, s, f, fill, tracking=0, anchor="ls"):
    """Draw type. Refuses an accent fill: there are no gold words in this system."""
    if tuple(fill) in RAMP:
        raise AssertionError(
            f"the accent means 'this one is following you' and may not be used for "
            f"type; {s!r} was asked for {fill}"
        )
    _guard(fill)
    if not tracking:
        d.text((x, y), s, font=f, fill=fill, anchor=anchor)
        return measure(d, s, f)
    cx = x
    for ch in s:
        d.text((cx, y), ch, font=f, fill=fill, anchor=anchor)
        cx += d.textlength(ch, font=f) + tracking
    return cx - x - tracking


def measure(d, s, f, tracking=0):
    w = d.textlength(s, font=f)
    return w + tracking * max(len(s) - 1, 0)


# --- the measured object ---------------------------------------------------
def decimate(pts, every):
    """Every Nth real sample. NOT a smoothed or averaged curve: each point
    plotted is a value the firmware actually produces. At banner scale 300
    samples of +/-4 dB jitter render as fur and hide the one thing the picture
    is for, which is that the gold trace keeps climbing."""
    return pts[::every] + ([pts[-1]] if pts and len(pts) % every else [])


def dwell_plot(d, box, theme, ss, label_font, mono_font, safe=None):
    """The scenario's traces: everyone in the room, and the one that stays.

    x is time across the scenario, y is signal strength. The accent draws the
    Find My tag that trips the follow heuristic and the vertical marker at the
    moment it trips - and nothing else, so the picture says what the product
    says.
    """
    x0, y0, x1, y1 = box
    loop = demo_loop_ms()
    cast = demo_cast()

    lo, hi = -95.0, -50.0  # dBm bounds of the plot

    def px(t_ms):
        return x0 + (x1 - x0) * (t_ms / loop)

    def py(rssi):
        v = (max(lo, min(hi, rssi)) - lo) / (hi - lo)
        return y1 - (y1 - y0) * v

    # Range gridlines - structure only, never the accent.
    for db in (-90, -80, -70, -60):
        y = py(db)
        d.line([(x0, y), (x1, y)], fill=theme["rule"], width=max(1, ss // 2))
        text(
            d,
            x0 - 10 * ss,
            y + 4 * ss,
            f"{db}",
            mono_font,
            theme["second"],
            anchor="rs",
        )

    every = 8  # 250 ms per tick, so one point every 2 s
    hero = None
    for dev in cast:
        pts = rssi_series(dev)
        if len(pts) < 2:
            continue
        trip = alert_moment(dev)
        is_hero = dev["type"] == "TrackerTypeAppleFindMy" and trip is not None
        if is_hero:
            hero = (dev, pts, trip)
            continue
        xy = [(px(t), py(r)) for t, r in decimate(pts, every)]
        d.line(xy, fill=theme["quiet"], width=max(1, int(1.2 * ss)), joint="curve")

    if hero is None:
        raise AssertionError(
            "the demo cast no longer contains a Find My tag that trips the follow "
            "heuristic, so the banner has nothing to measure - check demo_source.c"
        )

    dev, pts, trip = hero
    hero_pts = decimate(pts, every)
    xy = [(px(t), py(r)) for t, r in hero_pts]
    d.line(xy, fill=_guard(theme["accent"]), width=max(3, int(3.6 * ss)), joint="curve")

    # The moment the app calls it: a full-height vertical and a ringed node.
    tx = px(trip)
    ty = py(next(r for t, r in hero_pts if t >= trip))
    for yy in range(int(y0), int(y1), int(7 * ss)):
        d.line(
            [(tx, yy), (tx, min(yy + 3 * ss, y1))],
            fill=theme["accent"],
            width=max(1, ss),
        )
    r = int(5.0 * ss)
    d.ellipse([tx - r, ty - r, tx + r, ty + r], fill=theme["accent"])
    r2 = int(9.5 * ss)
    d.ellipse(
        [tx - r2, ty - r2, tx + r2, ty + r2], outline=theme["accent"], width=max(1, ss)
    )

    # Caption under the marker, clamped so a caption wider than the thing it
    # labels cannot walk out of the safe box.
    cap = f"FOLLOWING  {trip / 1000:.0f}s"
    cw = measure(d, cap, label_font)
    cx = tx - cw / 2
    if safe is not None:
        cx = min(max(cx, safe), d.im.size[0] - safe - cw)
    cy = y1 + 26 * ss
    text(d, cx, cy, cap, label_font, theme["ink"])
    if safe is not None:
        return (cx, cy - 12 * ss, cx + cw, cy + 4 * ss)
    return None


def ring_field(size, theme, cx, cy, ss, rings=9, step=46):
    """Range rings as atmosphere. Never registered with the SafeBox: this is
    background art and is meant to bleed off the canvas."""
    layer = Image.new("RGB", size, theme["ground"])
    dd = ImageDraw.Draw(layer)
    for i in range(rings):
        r = (i + 1) * step * ss
        dd.ellipse(
            [cx - r, cy - r, cx + r, cy + r],
            outline=theme["rule"],
            width=max(1, ss // 2),
        )
    return layer


def edge_fade(size, inset):
    """Ramp the art to nothing before the edges.

    A banner may bleed - nothing crops a README. A social card may not: every
    surface that shows one (Twitter, Discord, Slack, LinkedIn) crops it to its
    own aspect ratio, so a ring sliced mid-arc reads as a rendering fault. This
    also makes the pixel-level safe-border assert mean something, because it
    cannot otherwise tell atmosphere from information.
    """
    w, h = size
    m = Image.new("L", size, 255)
    px = m.load()
    for x in range(w):
        fx = min(x, w - 1 - x)
        for y in range(h):
            fy = min(y, h - 1 - y)
            f = min(fx, fy)
            px[x, y] = 255 if f >= inset else int(255 * (f / inset) ** 1.6)
    return m


def fade_mask(size, stops):
    """A greyscale mask that ramps to zero across the type column.

    Shrinking the art to get out of the type's way is the wrong move - it gives
    up the scale that makes it work. Masking it off the words keeps both.
    """
    m = Image.new("L", size, 255)
    px = m.load()
    w, h = size
    for x in range(w):
        v = 255
        for x_from, x_to, v_from, v_to in stops:
            if x_from <= x <= x_to:
                t = (x - x_from) / max(x_to - x_from, 1)
                v = int(v_from + (v_to - v_from) * t)
                break
            if x < x_from:
                v = v_from
                break
        for y in range(h):
            px[x, y] = v
    return m


def paste_capture(out, name, x, y, scale, safe=None):
    """Paste a real device capture at INTEGER scale, or return None if absent.

    Integer + NEAREST or it stops looking like a screenshot and starts looking
    like a drawing of one. This is also the only place ember is allowed on an
    asset, because these pixels are light the device actually emitted.
    """
    p = os.path.join(SHOTS, f"{name}.png")
    if not os.path.exists(p):
        return None
    im = Image.open(p).convert("RGB")
    # Captures are saved at 4x; take them back to 1x then up by an integer.
    base = im.resize((128, 64), Image.NEAREST)
    big = base.resize((128 * scale, 64 * scale), Image.NEAREST)
    out.paste(big, (int(x), int(y)))
    return (x, y, x + big.width, y + big.height)


# --- banner: 2560x800, authored on a 1280x400 grid --------------------------
def render_banner(path, theme, W=2560, H=800, SS=2):
    ss = SS
    ground = theme["ground"]

    art = ring_field((W, H), theme, cx=int(W * 0.80), cy=int(H * 0.50), ss=ss)
    out = Image.new("RGB", (W, H), ground)
    # Kill the rings across the wordmark column and let them live on the right.
    mask = fade_mask(
        (W, H), [(0, int(W * 0.30), 0, 0), (int(W * 0.30), int(W * 0.46), 0, 255)]
    )
    flat = Image.new("RGB", (W, H), ground)
    out = Image.composite(art, flat, mask)

    d = ImageDraw.Draw(out)
    safe = 56 * ss  # a banner has no crop, but keep it off the very edge
    box = SafeBox(W, H, safe)

    f_name = font(SERIF, 104 * ss, index=2)
    f_tag = font(SERIF, 34 * ss, index=0)
    f_desc = font(SANS, 21 * ss)
    f_small = font(MONO, 17 * ss)

    x = 80 * ss
    w = text(d, x, 176 * ss, "GHOSTTAG", f_name, theme["ink"], tracking=5 * ss)
    box.add(x, 176 * ss - 78 * ss, x + w, 176 * ss + 8 * ss, "wordmark")
    # The plot column starts clear of the MEASURED wordmark, not of a guess at
    # how wide "GHOSTTAG" comes out at this size in this face.
    type_right = x + w

    w = text(d, x, 232 * ss, "hunt the tags that haunt you", f_tag, theme["second"])
    box.add(x, 232 * ss - 26 * ss, x + w, 232 * ss + 8 * ss, "tagline")

    d.line(
        [(x, 266 * ss), (x + 300 * ss, 266 * ss)], fill=theme["rule"], width=max(1, ss)
    )

    w = text(
        d, x, 306 * ss, "Anti-stalking BLE tracker detection", f_desc, theme["ink"]
    )
    box.add(x, 306 * ss - 16 * ss, x + w, 306 * ss + 6 * ss, "descriptor")
    w2 = text(d, x, 336 * ss, "for Flipper Zero", f_desc, theme["second"])
    box.add(x, 336 * ss - 16 * ss, x + w2, 336 * ss + 6 * ss, "descriptor2")

    # The measured object, starting clear of the type column.
    plot_left = max(int(W * 0.52), int(type_right + 70 * ss))
    plot = (plot_left, 150 * ss, int(W - 80 * ss), 280 * ss)
    cap_box = dwell_plot(d, plot, theme, ss, f_small, f_small, safe=safe)
    box.add(plot[0], plot[1], plot[2], plot[3], "dwell plot")
    if cap_box:
        box.add(*cap_box, "plot caption")

    lab = "SIGNAL (dBm) ACROSS ONE DEMO SCENARIO"
    lw = text(d, plot[0], plot[1] - 26 * ss, lab, f_small, theme["second"], tracking=ss)
    box.add(plot[0], plot[1] - 38 * ss, plot[0] + lw, plot[1] - 18 * ss, "plot label")

    box.no_overlap(
        ignore=(("dwell plot", "plot caption"), ("dwell plot", "plot label"))
    )
    m, what = box.check()
    out.save(path)
    print(
        f"wrote {os.path.relpath(path, HERE)}  ({W}x{H}, tightest margin {int(m)}px at {what})"
    )


# --- social card: 1280x640, authored inside a 96px cushion ------------------
def render_card(path, W=1280, H=640, SS=2):
    """GitHub's own guidance is a 40pt (80px) border. Author at 96, assert at 80.

    Authoring on the limit fails the limit by a pixel or three every time: a
    glyph's left bearing puts ink outside its anchor, and a thick line's end
    cap overshoots both ends of its path.
    """
    ss = SS
    theme = THEME["light"]
    ground = theme["ground"]
    Wb, Hb = W * ss, H * ss

    art = ring_field(
        (Wb, Hb), theme, cx=int(Wb * 0.50), cy=int(Hb * 0.44), ss=ss, step=38
    )
    flat = Image.new("RGB", (Wb, Hb), ground)
    mask = fade_mask(
        (Wb, Hb),
        [(0, int(Wb * 0.10), 60, 60), (int(Wb * 0.10), int(Wb * 0.50), 60, 255)],
    )
    mask = ImageChops.multiply(mask, edge_fade((Wb, Hb), inset=118 * ss))
    out = Image.composite(art, flat, mask)
    d = ImageDraw.Draw(out)

    authored = 96 * ss
    box = SafeBox(Wb, Hb, authored)

    f_name = font(SERIF, 74 * ss, index=2)
    f_tag = font(SERIF, 26 * ss, index=0)
    f_desc = font(SANS, 19 * ss)
    f_small = font(MONO, 15 * ss)

    x = authored
    # Smaller than the banner's: the card is half the width and the plot has to
    # sit beside it rather than under it.
    w = text(d, x, 214 * ss, "GHOSTTAG", f_name, theme["ink"], tracking=3 * ss)
    box.add(x, 214 * ss - 72 * ss, x + w, 214 * ss + 8 * ss, "wordmark")
    type_right = x + w

    w = text(d, x, 258 * ss, "hunt the tags that haunt you", f_tag, theme["second"])
    box.add(x, 258 * ss - 23 * ss, x + w, 258 * ss + 7 * ss, "tagline")

    d.line(
        [(x, 288 * ss), (x + 260 * ss, 288 * ss)], fill=theme["rule"], width=max(1, ss)
    )

    w = text(
        d, x, 326 * ss, "Finds the AirTag that is travelling", f_desc, theme["ink"]
    )
    box.add(x, 326 * ss - 17 * ss, x + w, 326 * ss + 6 * ss, "line1")
    w = text(d, x, 356 * ss, "with you. Flipper Zero + ESP32.", f_desc, theme["ink"])
    box.add(x, 356 * ss - 17 * ss, x + w, 356 * ss + 6 * ss, "line2")

    plot = (
        max(int(Wb * 0.50), int(type_right + 60 * ss)),
        190 * ss,
        Wb - authored,
        330 * ss,
    )
    cap_box = dwell_plot(d, plot, theme, ss, f_small, f_small, safe=authored)
    box.add(plot[0], plot[1], plot[2], plot[3], "dwell plot")
    if cap_box:
        box.add(*cap_box, "plot caption")

    # Derive the footer from the SAFE BOX and measure the glyphs, rather than
    # deriving it from the canvas height and assuming a line height. Sitting
    # the BASELINE on the safe line puts every descender outside it - which is
    # exactly what the check caught the first time this ran.
    box.no_overlap(ignore=(("dwell plot", "plot caption"),))

    foot = f"github.com/at0m-b0mb/GhostTag-FlipperZero   v{version()}"
    bb = d.textbbox((0, 0), foot, font=f_small, anchor="ls")
    descent = max(bb[3], 0)
    fy = Hb - authored - descent
    fw = text(d, x, fy, foot, f_small, theme["second"], tracking=ss)
    box.add(x, fy + bb[1], x + fw, fy + bb[3], "footer")

    m, what = box.check()
    out = out.resize((W, H), Image.LANCZOS)
    out.save(path)
    assert_safe_border(path, 80)
    print(
        f"wrote {os.path.relpath(path, HERE)}  ({W}x{H}, authored margin {int(m/ss)}px at {what})"
    )


def assert_safe_border(path, margin):
    """Measure the RENDERED file, not the layout constants.

    The renderer supersamples and rescales, so the margin in the code is not
    the margin in the file. This is the check that actually holds.
    """
    im = Image.open(path).convert("RGB")
    W, H = im.size
    px = im.load()
    bg = px[2, 2]

    def differs(x, y):
        c = px[x, y]
        return abs(c[0] - bg[0]) + abs(c[1] - bg[1]) + abs(c[2] - bg[2]) > 24

    cols = [x for x in range(W) if any(differs(x, y) for y in range(0, H, 2))]
    rows = [y for y in range(H) if any(differs(x, y) for x in range(0, W, 2))]
    if not cols or not rows:
        raise AssertionError(f"{path} appears to be blank")
    l, r, t, b = min(cols), max(cols), min(rows), max(rows)
    worst = min(l, W - 1 - r, t, H - 1 - b)
    if worst < margin:
        raise AssertionError(
            f"{os.path.basename(path)} has ink {worst}px from an edge "
            f"(left {l}, right {W-1-r}, top {t}, bottom {H-1-b}); "
            f"GitHub's 40pt safe border needs {margin}px."
        )
    print(f"    safe border ok: left {l}, right {W-1-r}, top {t}, bottom {H-1-b}")


# --- mark: the ping and the one that did not leave --------------------------
def render_mark(path, size=512, theme_name="light"):
    ss = 4
    theme = THEME[theme_name]
    S = size * ss
    out = Image.new("RGB", (S, S), theme["ground"])
    d = ImageDraw.Draw(out)
    c = S // 2

    for i in range(4):
        r = int(S * (0.11 + i * 0.105))
        d.ellipse(
            [c - r, c - r, c + r, c + r],
            outline=theme["rule"],
            width=max(2, int(S * 0.008)),
        )

    # Quiet contacts, then the one that stayed - the accent, again meaning
    # exactly one thing.
    quiet = [(0.62, 0.20), (2.35, 0.33), (3.9, 0.43), (5.15, 0.27)]
    for ang, rad in quiet:
        import math

        x = c + math.cos(ang) * S * rad
        y = c + math.sin(ang) * S * rad
        r = int(S * 0.016)
        d.ellipse([x - r, y - r, x + r, y + r], fill=theme["second"])

    import math

    ang, rad = 5.62, 0.155
    x = c + math.cos(ang) * S * rad
    y = c + math.sin(ang) * S * rad
    r = int(S * 0.036)
    d.ellipse([x - r, y - r, x + r, y + r], fill=_guard(theme["accent"]))
    r2 = int(S * 0.066)
    d.ellipse(
        [x - r2, y - r2, x + r2, y + r2],
        outline=theme["accent"],
        width=max(2, int(S * 0.009)),
    )

    out = out.resize((size, size), Image.LANCZOS)
    out.save(path)
    print(f"wrote {os.path.relpath(path, HERE)}  ({size}x{size})")
    return out


def main():
    for name, theme in THEME.items():
        render_banner(os.path.join(OUT, f"banner{theme['suffix']}.png"), theme)

    render_card(os.path.join(OUT, "social-preview.png"))

    mark = render_mark(os.path.join(OUT, "mark.png"), 512)
    for px in (180, 64):
        mark.resize((px, px), Image.LANCZOS).save(os.path.join(OUT, f"mark-{px}.png"))
        print(f"wrote images/mark-{px}.png  ({px}x{px})")
    # 32 and 16 are drawn natively - a downscale of a 512 turns to mush.
    for px in (32, 16):
        render_mark(os.path.join(OUT, f"mark-{px}.png"), px)

    print(
        f"\nGhostTag v{version()} - dwell window {demo_follow_ms()/1000:.0f}s, "
        f"{min_detections()} sightings minimum; every number above was read "
        f"out of the source, not typed here."
    )


if __name__ == "__main__":
    main()

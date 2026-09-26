#!/usr/bin/env python3
"""Render Faraday's brand assets: banner (light + dark), social card, and mark.

THE SHIELD
----------
Faraday measures how much of a signal a pouch keeps OUT, so the artwork is
built on the same idea: a field on one side of a boundary and nothing on the
other. Paper is the measured side - a grade is something you write down and
hand to someone. Black is the sealed side.

Two laws, both enforced below rather than merely intended. They are inherited
from this family of repositories because they are what stops a brand asset
from quietly lying about the product.

  LAW 1, THE ORANGE QUARANTINE. EMBER (#FE8A2C) is never a stroke this file
  draws. It is not a chosen colour: every capture in screenshots/ is exactly
  two colours, (254,138,44) and (0,0,0), so it is literally the value the
  device emits. The only ember on any asset is light the device actually
  emitted - pixels inside a real capture, pasted untouched at an integer
  scale. _guard() raises if EMBER reaches a draw call.

  LAW 2, THE ACCENT MEANS ONE THING. The accent family draws MEASUREMENT and
  nothing else - never a rule, a frame, a margin, a mount or a word. There are
  no gold words anywhere in this system. text() raises if an accent token is
  passed as a text fill. The banner therefore carries exactly one
  accent-coloured object, and that object is the instrument's own scale.

Nothing here is typed that could be read: the version comes from faraday_i.h,
the grade thresholds come from helpers/fdy_grade.h, the lock threshold comes
from the Sub-GHz scene, and the device screen is a real capture.

    python3 tools_gen_banner.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

from tools_brand_data import capture, grade_scale, signal_margin_db, version

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "images")
os.makedirs(OUT, exist_ok=True)

# --- palette ---------------------------------------------------------------
PAPER = (243, 241, 236)  # #F3F1EC  light ground, flat, never a gradient
BLACK = (0, 0, 0)  # #000000  true black - explicitly not navy
INK = (22, 19, 13)  # #16130D  identity type on paper (warm, in the brass family)
GRAPHITE = (87, 82, 74)  # #57524A  secondary type on paper
HAIRLINE = (217, 212, 200)  # #D9D4C8  rules and mounts on paper - structure only
BONE = (243, 241, 236)  # #F3F1EC  identity type on black
ASH = (194, 187, 177)  # #C2BBB1  secondary type on black - lifted for true black
SCORE = (42, 39, 36)  # #2A2724  rules and mounts on black - structure only

EMBER = (254, 138, 44)  # #FE8A2C  QUARANTINED. The device's own light. Never drawn.
AMBER = (216, 134, 42)  # #D8862A  measurement on black
BRASS = (176, 125, 34)  # #B07D22  measurement, lighter step
BRASS_DEEP = (138, 103, 20)  # #8A6714  measurement on paper

RAMP = {EMBER, AMBER, BRASS, BRASS_DEEP}

THEME = {
    "light": dict(
        ground=PAPER, ink=INK, second=GRAPHITE, rule=HAIRLINE,
        meas=BRASS_DEEP, meas_soft=BRASS, suffix="",
    ),
    "dark": dict(
        ground=BLACK, ink=BONE, second=ASH, rule=SCORE,
        meas=AMBER, meas_soft=BRASS_DEEP, suffix="-dark",
    ),
}

# --- type ------------------------------------------------------------------
# Didot because measuring instruments - galvanometers, theodolites, field
# strength meters - had their nameplates engraved in Didone, and Faraday is an
# instrument. Andale Mono for figures because tools_screenshot.py already uses
# it to caption device captures, so the brand's technical type and the
# device's technical type are the same shapes.
SERIF = "/System/Library/Fonts/Supplemental/Didot.ttc"  # 0 Regular, 1 Italic, 2 Bold
SANS = "/System/Library/Fonts/Supplemental/Futura.ttc"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"


def font(path, px, index=0):
    """No silent fallback. Catching OSError and substituting Arial would render
    the whole system in the one typeface it exists to replace, and look merely
    slightly wrong rather than obviously broken."""
    return ImageFont.truetype(path, px, index=index)


def _guard(fill):
    if tuple(fill) == EMBER:
        raise AssertionError("LAW 1: EMBER is never drawn - it may only be pasted from a capture")
    return fill


def measure(d, s, f, tracking=0):
    return sum(d.textlength(c, font=f) for c in s) + tracking * max(0, len(s) - 1)


def text(d, x, y, s, f, fill, tracking=0, anchor="ls"):
    """Baseline-anchored text with letter-spacing. Refuses accent colours."""
    if tuple(fill) in RAMP:
        raise AssertionError(f"LAW 2: the accent draws measurement, not words ({s!r})")
    w = measure(d, s, f, tracking)
    if anchor == "rs":
        x -= w
    elif anchor == "ms":
        x -= w / 2.0
    for ch in s:
        d.text((x, y), ch, font=f, fill=fill, anchor="ls")
        x += d.textlength(ch, font=f) + tracking
    return w


def paste_capture(out, path, x, y, scale, rule):
    """A device capture at an exact integer scale, NEAREST both ways.

    Composited AFTER the supersample collapse. Doing it before, or at a
    non-integer scale, or with any other resampling filter, gives a softened
    device screen that looks fine at review size and obviously wrong at full
    resolution - and softening it would also break LAW 1, because a blurred
    edge invents colours between ember and ink that the device never emitted.
    """
    cap = (
        Image.open(path)
        .convert("RGB")
        .resize((128, 64), Image.NEAREST)  # exact decimation of the 4x file
        .resize((128 * scale, 64 * scale), Image.NEAREST)
    )
    w, h = cap.size
    ImageDraw.Draw(out).rectangle([x - 3, y - 3, x + w + 2, y + h + 2], outline=rule, width=3)
    out.paste(cap, (int(x), int(y)))
    return w, h


def attenuation_scale(d, t, x0, x1, y, scale, margin_db, ss):
    """THE SIGNATURE ELEMENT: Faraday's own grading scale, drawn as an instrument.

    Not ornament and not a number anyone typed into a drawing - every tick
    below is a #define the firmware branches on, read out of
    helpers/fdy_grade.h by tools_brand_data.py. It is the only
    accent-coloured object on the banner, which is LAW 2 in practice: the gold
    marks measurement and nothing else.

    The axis runs 0 dB to the A+ threshold, and each grade's lower bound gets a
    tick and a letter. The zone the app refuses to grade at all is hatched
    rather than labelled on the axis: at the current thresholds the refusal
    margin and grade C's boundary are the same number, so a second tick there
    would have drawn one mark on top of another and said nothing.
    """
    top = scale[0][1]  # the A+ threshold is the end of the axis
    span = float(top)
    mono_tick = font(MONO, int(30 * ss))
    mono_cap = font(MONO, int(26 * ss))

    def px(db):
        return x0 + (x1 - x0) * (db / span)

    # the refused zone, hatched under the axis
    xm = px(margin_db)
    step = int(14 * ss)
    for hx in range(int(x0), int(xm), step):
        d.line([hx, y + int(16 * ss), hx + int(7 * ss), y + int(4 * ss)],
               fill=_guard(t["meas_soft"]), width=int(2 * ss))

    # the axis itself
    d.line([x0, y, x1, y], fill=_guard(t["meas_soft"]), width=int(3 * ss))

    for letter, db in reversed(scale):
        x = px(db)
        d.line([x, y - int(18 * ss), x, y + int(18 * ss)], fill=_guard(t["meas"]), width=int(4 * ss))
        text(d, x, y - int(30 * ss), letter, mono_tick, t["ink"], anchor="ms")
        text(d, x, y + int(54 * ss), f"{db}", mono_cap, t["second"], anchor="ms")

    text(d, x0, y + int(54 * ss), "dB", mono_cap, t["second"], anchor="ms")


def render(theme_name, W=2560, H=800, ss=2, want_boxes=False):
    """Supersampled render, collapsed, then the capture pasted at integer scale."""
    t = THEME[theme_name]
    big = Image.new("RGB", (W * ss, H * ss), t["ground"])
    d = ImageDraw.Draw(big)

    ver = version()
    scale = grade_scale()
    margin = signal_margin_db()

    M = int(150 * ss)  # margin
    mono_rail = font(MONO, int(30 * ss))

    # --- top rail -----------------------------------------------------------
    text(d, M, int(150 * ss), "SUB-GHZ + NFC", mono_rail, t["second"], tracking=int(7 * ss))
    text(d, M + int(430 * ss), int(150 * ss), "LISTEN-ONLY", mono_rail, t["second"], tracking=int(7 * ss))
    text(d, M + int(830 * ss), int(150 * ss), "NEVER TRANSMITS", mono_rail, t["second"], tracking=int(7 * ss))
    text(d, W * ss - M, int(150 * ss), "FLIPPER ZERO", mono_rail, t["second"], tracking=int(7 * ss), anchor="rs")
    d.line([M, int(186 * ss), W * ss - M, int(186 * ss)], fill=t["rule"], width=int(2 * ss))

    # --- wordmark -----------------------------------------------------------
    # Didot at this size needs its strokes to survive true black; the weight is
    # carried by the Bold face rather than by a synthetic stroke.
    text(d, M - int(6 * ss), int(390 * ss), "FARADAY", font(SERIF, int(210 * ss), index=2),
         t["ink"], tracking=int(6 * ss))

    # --- tagline ------------------------------------------------------------
    text(d, M, int(470 * ss), "Prove your pouch works.", font(SANS, int(64 * ss), index=1), t["ink"])
    text(d, M, int(528 * ss), "Two captures and a grade. Real dB, measured on the device.",
         font(SANS, int(38 * ss)), t["second"])
    text(d, M, int(578 * ss),
         f"It will not grade a baseline under {margin} dB, because noise is not a fob.",
         font(SANS, int(38 * ss)), t["second"])

    # --- the instrument scale ----------------------------------------------
    attenuation_scale(d, t, M + int(6 * ss), M + int(1180 * ss), int(666 * ss), scale, margin, ss)

    # --- bottom rail --------------------------------------------------------
    d.line([M, int(724 * ss), W * ss - M, int(724 * ss)], fill=t["rule"], width=int(2 * ss))
    text(d, M, int(768 * ss), "github.com/at0m-b0mb/Faraday-FlipperZero", mono_rail, t["second"])
    text(d, W * ss - M, int(768 * ss), f"MIT  ·  by at0m-b0mb  ·  v{ver}", mono_rail, t["second"],
         anchor="rs")

    out = big.resize((W, H), Image.LANCZOS)

    # --- the device's own screen, last and untouched -------------------------
    cap = capture("verdict.png", "shielded.png", "baseline.png")
    cw, ch = 128 * 6, 64 * 6
    cx, cy = W - 150 - cw, 236
    paste_capture(out, cap, cx, cy, 6, t["rule"])
    ImageDraw.Draw(out).text(
        (W - 150 - cw, 236 + ch + 34),
        "a real screen, captured off the device over USB",
        font=font(MONO, 24), fill=t["second"], anchor="ls",
    )
    if want_boxes:
        return out, [(cx, cy, cw, ch)]
    return out


# GitHub's repo-card geometry. The canvas is 1280x640 and GitHub asks for a
# 40pt border - 80px at this 2x canvas - around anything that matters, because
# Twitter, Discord, Slack and LinkedIn each crop the card to their own aspect
# ratio. Background may bleed to the edge; information may not.
SOCIAL_W, SOCIAL_H = 1280, 640
SOCIAL_SAFE = 80

# Lay out a few pixels INSIDE the safe box. A rule drawn exactly on the
# boundary comes back 77px from the edge after the supersampled LANCZOS
# collapse, because the filter spreads a hard edge over neighbouring pixels -
# so the file breaks the rule the layout satisfied.
SOCIAL_PAD = 8


def social(theme_name="light", W=SOCIAL_W, H=SOCIAL_H):
    """The GitHub social preview card.

    Laid out from the SAFE BOX, not from the canvas. The previous version
    positioned its footer relative to the canvas height and shipped with a
    36px bottom margin - fine in the file, cropped away on Discord.
    """
    t = THEME[theme_name]
    ss = 2
    big = Image.new("RGB", (W * ss, H * ss), t["ground"])
    d = ImageDraw.Draw(big)

    # the safe box, in supersampled coordinates
    inset = (SOCIAL_SAFE + SOCIAL_PAD) * ss
    L, R = inset, W * ss - inset
    T, B = inset, H * ss - inset

    ver = version()
    mono_rail = font(MONO, int(26 * ss))

    # --- top rail, on the safe box's top edge -------------------------------
    text(d, L, T + int(20 * ss), "FLIPPER ZERO", mono_rail, t["second"], tracking=int(6 * ss))
    text(d, L + int(300 * ss), T + int(20 * ss), "SHIELDING TESTER", mono_rail, t["second"],
         tracking=int(6 * ss))
    text(d, R, T + int(20 * ss), f"v{ver}", mono_rail, t["second"], tracking=int(6 * ss),
         anchor="rs")
    d.line([L, T + int(44 * ss), R, T + int(44 * ss)], fill=t["rule"], width=int(2 * ss))

    # --- wordmark and tagline, left column ----------------------------------
    # The capture occupies the right of the card, so the wordmark has to live
    # in what is left. Measured, not assumed: at a 4x capture this column was
    # 592px wide, "FARADAY" rendered 595px, and the card shipped reading
    # "FARADA" with the Y hidden behind the device screen.
    col = (W - SOCIAL_SAFE - SOCIAL_PAD - 128 * 3 - 24) * ss - L
    wm_font = font(SERIF, int(120 * ss), index=2)  # 648px in a 696px column
    if measure(d, "FARADAY", wm_font, int(4 * ss)) > col:
        raise AssertionError("the wordmark does not fit beside the capture")
    text(d, L - int(5 * ss), T + int(210 * ss), "FARADAY", wm_font, t["ink"], tracking=int(4 * ss))
    text(d, L, T + int(266 * ss), "Prove your pouch works.", font(SANS, int(46 * ss), index=1),
         t["ink"])
    text(d, L, T + int(312 * ss), "Real dB attenuation, measured on the", font(SANS, int(30 * ss)),
         t["second"])
    text(d, L, T + int(350 * ss), "device. Graded A+ to F.", font(SANS, int(30 * ss)), t["second"])

    # --- bottom rail, on the safe box's bottom edge -------------------------
    d.line([L, B - int(44 * ss), R, B - int(44 * ss)], fill=t["rule"], width=int(2 * ss))
    text(d, L, B - int(10 * ss), "github.com/at0m-b0mb/Faraday-FlipperZero", mono_rail,
         t["second"])
    text(d, R, B - int(10 * ss), "MIT  ·  never transmits", mono_rail, t["second"], anchor="rs")

    out = big.resize((W, H), Image.LANCZOS)

    # --- the device's own screen, pasted last and untouched ------------------
    cap = capture("verdict.png", "band.png", "shielded.png", "baseline.png")
    scale = 3
    cw, ch = 128 * scale, 64 * scale
    cx = W - SOCIAL_SAFE - SOCIAL_PAD - cw
    cy = (H - ch) // 2 + 10
    paste_capture(out, cap, cx, cy, scale, t["rule"])
    return out, [(cx, cy, cw, ch)]


def assert_safe_border(img, safe=SOCIAL_SAFE):
    """GitHub's 40pt rule, checked on the pixels rather than on the layout.

    The renderer supersamples and rescales, so the margin in the source is not
    the margin in the file - this measures the actual inked extent.
    """
    im = img.convert("RGB")
    W, H = im.size
    px = im.load()
    bg = px[2, 2]
    xs, ys = [], []
    for y in range(H):
        for x in range(W):
            if px[x, y] != bg:
                xs.append(x)
                ys.append(y)
    if not xs:
        raise AssertionError("social card is blank")
    l, r, tp, b = min(xs), max(xs), min(ys), max(ys)
    margins = {"left": l, "right": W - 1 - r, "top": tp, "bottom": H - 1 - b}
    bad = {k: v for k, v in margins.items() if v < safe}
    if bad:
        raise AssertionError(
            f"social card breaks GitHub's {safe}px safe border: {bad} "
            f"(canvas {W}x{H}, ink x {l}..{r} y {tp}..{b})"
        )
    return margins


def mark(px, theme_name="light"):
    """The app mark: a sealed pouch with the field stopped at its wall.

    Drawn, not photographed, and deliberately not the device icon: this is the
    only piece of artwork in the system that is an idea rather than a
    measurement, so it uses structure colours only.
    """
    t = THEME[theme_name]
    ss = 8
    S = px * ss
    img = Image.new("RGB", (S, S), t["ground"])
    d = ImageDraw.Draw(img)
    cx, cy = S // 2, int(S * 0.52)
    w, h = int(S * 0.56), int(S * 0.44)
    r = int(S * 0.07)
    # waves inside, stopped by the wall
    for i in range(3):
        rad = int(S * (0.07 + i * 0.055))
        d.ellipse([cx - rad, cy - rad, cx + rad, cy + rad], outline=t["second"], width=max(1, int(S * 0.012)))
    d.rounded_rectangle([cx - w // 2, cy - h // 2, cx + w // 2, cy + h // 2], radius=r,
                        outline=t["ink"], width=max(1, int(S * 0.028)))
    # the seal
    for k in range(-2, 3):
        x = cx + k * int(S * 0.075)
        d.line([x, cy - h // 2 + int(S * 0.02), x, cy - h // 2 + int(S * 0.075)],
               fill=t["ink"], width=max(1, int(S * 0.018)))
    return img.resize((px, px), Image.LANCZOS)


def assert_quarantine(img, capture_boxes):
    """LAW 1, checked on the finished pixels rather than trusted.

    _guard() can only catch an accent passed to a draw call it wraps. This
    looks at the rendered image and asserts that the device's own orange
    appears ONLY inside the rectangles where a capture was pasted - so no
    drawing, gradient or resampling artefact anywhere else on the asset can
    impersonate light the device emitted.
    """
    px = img.convert("RGB").load()
    W, H = img.size
    for y in range(H):
        for x in range(W):
            if px[x, y] != EMBER:
                continue
            if not any(bx <= x < bx + bw and by <= y < by + bh for bx, by, bw, bh in capture_boxes):
                raise AssertionError(
                    f"LAW 1 violated: the device's ember appears at ({x},{y}), "
                    "outside every pasted capture"
                )


if __name__ == "__main__":
    for name, t in THEME.items():
        b, boxes = render(name, want_boxes=True)
        assert_quarantine(b, boxes)
        p = os.path.join(OUT, f"banner{t['suffix']}.png")
        b.save(p)
        print(f"wrote {os.path.relpath(p, HERE)}  ({b.size[0]}x{b.size[1]})  law 1 ok")

    card, boxes = social("light")
    assert_quarantine(card, boxes)
    m = assert_safe_border(card)
    p = os.path.join(OUT, "social-preview.png")
    card.save(p)
    print(
        f"wrote {os.path.relpath(p, HERE)}  ({card.size[0]}x{card.size[1]})  "
        f"law 1 ok  safe border ok (min {min(m.values())}px)"
    )

    for px in (16, 32, 64, 180):
        m = mark(px)
        p = os.path.join(OUT, f"mark-{px}.png")
        m.save(p)
    print("wrote images/mark-{16,32,64,180}.png")

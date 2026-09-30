#!/usr/bin/env python3
"""Render Specter's brand assets: banner (light + dark), social card, and mark.

BACKLIGHT
---------
The Flipper's backlight orange and this project's house gold are sixteen
degrees apart on the same hue wheel, so they are treated as one ramp read from
opposite ends: more INK means more signal on paper, more LIGHT means more
signal on black. Paper is where a sweep ends - Specter writes a timestamped
logbook you can hand to someone. Black is where it happens - Specter's stealth
mode forces the backlight off.

Two laws, both enforced below rather than merely intended:

  LAW 1, THE ORANGE QUARANTINE. EMBER (#FE8A2C) is never a stroke this file
  draws. It is not a chosen colour: every capture in screenshots/ is exactly
  two colours, (254,138,44) and (0,0,0), so it is literally the value the
  product emits. The only ember on any asset is light the device actually
  emitted - i.e. pixels inside a real qFlipper capture, pasted untouched.
  Artwork drawn here uses the ramp one stop cooler, so a drawn band can never
  impersonate the device's own screen. `_guard()` raises if EMBER reaches a
  draw call.

  LAW 2, THE ACCENT MEANS ONE THING. The accent family draws measured carrier
  and nothing else - never a rule, a frame, a margin, a mount or a word. There
  are no gold words anywhere in this system. `text()` raises if an accent token
  is passed as a text fill. The banner therefore has exactly one
  accent-coloured object on it, and that object is a measurement.

Nothing here is typed that could be read: the version comes from specter_i.h,
the waveform and both duty figures come from a hardware capture, and the trace
rows are asserted against views/fingerprint_view.c so a future layout change
fails loudly instead of drawing the wrong row.

    python3 tools_gen_banner.py
"""
import json
import os

from PIL import Image, ImageDraw, ImageFont

from tools_brand_data import carrier_from_capture, parse_defines, version

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
ASH = (169, 162, 154)  # #A9A29A  secondary type on black
SCORE = (42, 39, 36)  # #2A2724  rules and mounts on black - structure only

EMBER = (254, 138, 44)  # #FE8A2C  QUARANTINED. Sampled from the device. Never drawn.
AMBER = (216, 134, 42)  # #D8862A  carrier UP on black
BRASS = (176, 125, 34)  # #B07D22  carrier DOWN on paper
BRASS_DEEP = (138, 103, 20)  # #8A6714  carrier UP on paper / carrier DOWN on black

RAMP = {EMBER, AMBER, BRASS, BRASS_DEEP}

THEME = {
    "light": dict(
        ground=PAPER, ink=INK, second=GRAPHITE, rule=HAIRLINE, up=BRASS_DEEP, down=BRASS
    ),
    "dark": dict(
        ground=BLACK, ink=BONE, second=ASH, rule=SCORE, up=AMBER, down=BRASS_DEEP
    ),
}

# --- type ------------------------------------------------------------------
# Didot because instrument nameplates - galvanometers, theodolites - were
# engraved in Didone. Andale Mono because tools_gen_mockups.py already uses it
# to stand in for the device's own FontSecondary, so the brand's technical type
# and the device's technical type are the same shapes.
SERIF = "/System/Library/Fonts/Supplemental/Didot.ttc"  # 0 Regular, 2 Bold
SANS = "/System/Library/Fonts/Supplemental/Futura.ttc"  # NOT /System/Library/Fonts/
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"


def font(path, px, index=0):
    """No silent fallback. The previous version caught OSError and substituted
    Arial Bold, which would have rendered the whole system in the one typeface
    it exists to replace, and looked merely slightly wrong."""
    return ImageFont.truetype(path, px, index=index)


def _guard(fill):
    if tuple(fill) == EMBER:
        raise AssertionError(
            "LAW 1: EMBER is never drawn - it may only be pasted from a capture"
        )
    return fill


def text(d, x, y, s, f, fill, tracking=0, anchor="ls"):
    """Baseline-anchored text with letter-spacing. Refuses accent colours."""
    if tuple(fill) in RAMP:
        raise AssertionError(
            f"LAW 2: the accent draws measured carrier, not words ({s!r})"
        )
    w = measure(d, s, f, tracking)
    if anchor == "rs":
        x -= w
    elif anchor == "ms":
        x -= w / 2.0
    for ch in s:
        d.text((x, y), ch, font=f, fill=fill, anchor="ls")
        x += d.textlength(ch, font=f) + tracking
    return w


def measure(d, s, f, tracking=0):
    return sum(d.textlength(c, font=f) for c in s) + tracking * max(0, len(s) - 1)


def carrier_band(d, bits, x0, x1, y_hi, y_lo, up, down, hstroke, vstroke):
    """The signature element: one screenful of recorded carrier, magnified.

    LOW runs are drawn in the carrier-down token and HIGH runs plus every edge
    in carrier-up, so the ramp position itself says whether the carrier was up.
    Edges are never drawn at column 0, mirroring the `i > 0` guard in the view's
    own draw_trace()."""
    _guard(up), _guard(down)
    pitch = (x1 - x0) / len(bits)
    prev = None
    for i, b in enumerate(bits):
        xa, xb = x0 + i * pitch, x0 + (i + 1) * pitch
        y = y_hi if b else y_lo
        d.line([xa, y, xb, y], fill=(up if b else down), width=hstroke)
        if prev is not None and b != prev:
            d.line([xa, y_hi, xa, y_lo], fill=up, width=vstroke)
        prev = b
    return pitch


def paste_capture(out, name, x, y, scale):
    """A device capture at an exact integer scale, NEAREST both ways.

    Composited AFTER the supersample collapse. Doing it before, or at a
    non-integer scale, or with any other filter, gives a softened device screen
    that looks fine at review size and obviously wrong at full resolution."""
    cap = (
        Image.open(os.path.join(HERE, "screenshots", name))
        .convert("RGB")
        .resize((128, 64), Image.NEAREST)  # exact 4x decimation of the 512x256 file
        .resize((128 * scale, 64 * scale), Image.NEAREST)
    )
    out.paste(cap, (int(x), int(y)))
    return cap.size


# --- what the device printed on the capture ---------------------------------
# Read off screenshots/ss2.png by eye, because the firmware's own readouts are
# rendered as pixels and there is nothing to parse. They exist as the
# cross-check for the waveform extracted from that same image: if a re-shoot
# moves the measured duty more than a few points from DEVICE_UP_PCT, series()
# refuses to build rather than print a figure that disagrees with the screen it
# is standing next to. Update these together with the capture, never alone.
DEVICE_CLASS = "POLLING"
DEVICE_PERIOD_MS = 39  # "PER 39ms"
DEVICE_UP_PCT = 28  # "UP 28%"


def series():
    """The recorded carrier, plus the device's own printed duty for cross-check."""
    bits, hi, lo = carrier_from_capture()
    c_hi, c_lo = parse_defines("views/fingerprint_view.c", "TRACE_HI", "TRACE_LO")
    if abs(hi - c_hi) > 1 or abs(lo - c_lo) > 1:
        raise SystemExit(
            f"capture trace rows {hi}/{lo} vs source {c_hi}/{c_lo} - re-shoot screenshots/ss2.png"
        )
    duty = 100.0 * sum(bits) / len(bits)
    edges = sum(1 for i in range(1, len(bits)) if bits[i] != bits[i - 1])
    if edges < 12:
        raise SystemExit(
            f"only {edges} edges in the capture - not enough polls to show a cadence"
        )
    # The duty counter printed on that same screen by the firmware itself.
    device_up = DEVICE_UP_PCT
    if abs(duty - device_up) > 4:
        raise SystemExit(
            f"trace duty {duty:.1f}% has drifted from the device's {device_up}%"
        )
    info = dict(
        bits=bits,
        hi=hi,
        lo=lo,
        duty=round(duty, 1),
        edges=edges,
        device_up=device_up,
        version=version(),
        capture="screenshots/ss2.png",
        capture_mtime=os.path.getmtime(os.path.join(HERE, "screenshots", "ss2.png")),
    )
    with open(os.path.join(OUT, "series.json"), "w") as fh:
        json.dump(info, fh, indent=2)
    return info


# --- banner: 2560x800, authored on a 1280x400 grid -------------------------
def render_banner(path, theme, W=2560, H=800, SS=2):
    t = THEME[theme]
    K = W / 1280.0
    info = series()

    def u(v):
        return int(round(v * SS * K))

    img = Image.new("RGB", (int(W * SS), int(H * SS)), t["ground"])
    d = ImageDraw.Draw(img)

    f_kick = font(MONO, u(15))
    f_word = font(SERIF, u(116), index=0)
    f_tag = font(SANS, u(26))
    f_sub = font(SANS, u(18))
    f_foot = font(MONO, u(13))

    L, R = u(72), u(1208)

    # one rule, not a box
    d.line([L, u(56), R, u(56)], fill=_guard(t["rule"]), width=max(1, u(1)))

    # kicker: the only emphasis in the system is ink, not colour
    x = L
    x += text(d, x, u(82), "13.56 MHz   ·   ", f_kick, t["second"], u(5)) + u(5)
    x += text(d, x, u(82), "LISTEN-ONLY", f_kick, t["ink"], u(5)) + u(5)
    text(d, x, u(82), "   ·   NEVER TRANSMITS", f_kick, t["second"], u(5))
    text(d, R, u(82), "FLIPPER ZERO", f_kick, t["second"], u(5), anchor="rs")

    # the plate's mount - a specimen on a page, not a bezel
    d.rectangle(
        [u(818), u(94), u(1214), u(298)], outline=_guard(t["rule"]), width=max(1, u(2))
    )

    text(d, L, u(206), "SPECTER", f_word, t["ink"], u(13))
    text(d, L, u(244), "Sweep for the readers you can't see.", f_tag, t["ink"])
    text(
        d,
        L,
        u(272),
        "Find it · fingerprint it · survey the room · leave it on watch.",
        f_sub,
        t["second"],
    )

    carrier_band(
        d,
        info["bits"],
        L,
        R,
        u(314),
        u(350),
        t["up"],
        t["down"],
        max(1, u(5)),
        max(1, u(4)),
    )

    text(
        d,
        L,
        u(372),
        "github.com/at0m-b0mb/Specter-FlipperZero",
        f_foot,
        t["second"],
        u(2),
    )
    text(
        d,
        R,
        u(372),
        f"{DEVICE_CLASS} · PER {DEVICE_PERIOD_MS} ms · UP {info['device_up']}% DEVICE · "
        f"{round(info['duty'])}% THIS TRACE · v{info['version']}",
        f_foot,
        t["second"],
        u(2),
        anchor="rs",
    )

    out = img.resize((W, H), Image.LANCZOS)  # collapse supersampling FIRST
    scale = int(round(3 * K))  # 3 on the authoring grid, 6 at 2560
    paste_capture(out, "ss2.png", 824 * K, 100 * (H / 400.0), scale)
    assert out.getpixel((int(830 * K), int(110 * (H / 400.0)))) in (
        EMBER,
        BLACK,
    ), "the device plate was resampled - it must stay two-colour"
    out.save(path)
    print("wrote", path, out.size, f"[{theme}]")


# --- social card: 1280x640, authored 1:1 -----------------------------------
# GitHub's own numbers, from the Repo Card Template and the Settings copy:
# author at 1280x640, and leave a 40pt border - 80px on this 2x canvas -
# around anything that matters, because Twitter/X, Discord, Slack and LinkedIn
# each crop this card to their own aspect ratio. Background art may bleed to
# the edges. Information may not.
#
# The previous card ignored that on all four sides: the carrier ran full bleed
# (0px left and right), the eyebrow sat 35px down and the footer 18px up. It
# looked fine as a file and lost its URL on half the surfaces that matter.
CARD_SAFE = 80  # GitHub's requirement - what assert_safe_border checks
# Author at 96, not at 80. Type does not start exactly at its anchor (a glyph's
# left bearing can put ink a pixel outside it) and PIL centres a thick line on
# its path, so end caps overshoot too. Authoring on the limit therefore fails
# the limit - by 1 to 3px, which is exactly how this kind of thing ships. The
# 16px cushion is the difference between "inside the box" and "inside the box
# once it is actually rendered".
CARD_PAD = 96
CARD_L, CARD_R = CARD_PAD, 1280 - CARD_PAD  # 96 .. 1184
CARD_T, CARD_B = CARD_PAD, 640 - CARD_PAD  # 96 .. 544

# The capture, at an exact 4x, anchored to the right edge of the safe box.
CARD_CAP_SCALE = 4
CARD_CAP_W, CARD_CAP_H = 128 * CARD_CAP_SCALE, 64 * CARD_CAP_SCALE  # 512 x 256
CARD_CAP_X = CARD_R - CARD_CAP_W  # 688
CARD_CAP_Y = 176
CARD_GUTTER = 56  # between the wordmark column and the capture


def render_card(path, W=1280, H=640, SS=2):
    """The repo card: the banner's composition, re-cut for 1280x640.

    Same three elements in the same relationship - wordmark left, the device
    right, the recorded carrier along the bottom - so the card and the banner
    read as one identity rather than as two designs of the same name. The
    previous card stacked everything down the centre, which at the ~320px a
    feed actually renders is a column of small things with no hierarchy.
    """
    t = THEME["dark"]
    info = series()

    def u(v):
        return int(round(v * SS))

    img = Image.new("RGB", (W * SS, H * SS), t["ground"])
    d = ImageDraw.Draw(img)

    f_kick = font(MONO, u(15))
    # Didot BOLD here, and only here: Regular's thins at this size are about two
    # pixels, which is half a pixel once a feed scales the card to ~320px, and
    # they simply vanish. Changing weight to survive the medium is a typographic
    # decision; shipping an illegible thumbnail is not.
    f_word = font(SERIF, u(88), index=2)
    f_tag = font(SANS, u(25))
    f_sub = font(SANS, u(17))
    f_foot = font(MONO, u(14))

    # Structure only - a hairline is not information, so it may span the box.
    d.line([u(CARD_L), u(104), u(CARD_R), u(104)], fill=t["rule"], width=max(1, u(1)))

    text(
        d,
        u(CARD_L),
        u(140),
        "FLIPPER ZERO · 13.56 MHz · LISTEN-ONLY · NEVER TRANSMITS",
        f_kick,
        t["second"],
        u(5),
    )

    word_w = measure(d, "SPECTER", f_word, u(10)) / SS
    limit = CARD_CAP_X - CARD_GUTTER - CARD_L
    assert (
        word_w <= limit
    ), f"wordmark is {word_w:.0f}px but only {limit}px of column clears the capture"
    text(d, u(CARD_L), u(304), "SPECTER", f_word, t["ink"], u(10))
    text(
        d, u(CARD_L), u(350), "Sweep for the readers you can't see.", f_tag, t["second"]
    )
    # The four modes, in the banner's own words. This column would otherwise
    # run 120px of empty black between the tagline and the carrier, and on a
    # card the size of a feed thumbnail that space is the only chance to say
    # what the thing actually does.
    sub = "Find it · fingerprint it · survey the room · leave it on watch."
    sub_w = measure(d, sub, f_sub) / SS
    assert sub_w <= CARD_CAP_X - CARD_GUTTER - CARD_L, f"mode line is {sub_w:.0f}px"
    text(d, u(CARD_L), u(396), sub, f_sub, t["second"])

    # The signature element, and therefore INFORMATION: it stays inside the safe
    # box rather than bleeding, unlike on the banner where the canvas is the
    # README's own full width and nothing crops it.
    #
    # Inset by half a stroke. PIL centres a width-w line on the path, so a band
    # whose first column sits exactly on the safe edge puts three pixels of ink
    # outside it - which is precisely what the assert below caught, at 77px.
    band_pad = 6
    carrier_band(
        d,
        info["bits"],
        u(CARD_L + band_pad),
        u(CARD_R - band_pad),
        u(470),
        u(500),
        t["up"],
        t["down"],
        max(1, u(6)),
        max(1, u(5)),
    )

    text(
        d,
        u(CARD_L),
        u(544),
        "github.com/at0m-b0mb/Specter-FlipperZero",
        f_foot,
        t["second"],
        u(2),
    )
    text(
        d,
        u(CARD_R),
        u(544),
        f"v{info['version']} · MIT",
        f_foot,
        t["second"],
        u(2),
        anchor="rs",
    )

    out = img.resize((W, H), Image.LANCZOS)
    # No mount and no frame: the capture is two colours against true black and
    # defines its own edge, so a frame would be a box drawn around nothing.
    paste_capture(out, "ss0.png", CARD_CAP_X, CARD_CAP_Y, CARD_CAP_SCALE)
    out.save(path)
    assert_safe_border(path, CARD_SAFE)
    print("wrote", path, out.size, "[card]")


def assert_safe_border(path, margin):
    """Measure the RENDERED pixels, never the layout constants.

    The renderer supersamples and rescales, so the margin in the source is not
    the margin in the file - which is exactly how a card authored with a 110px
    margin once shipped with a 36px one. This is the check that would have
    caught it.
    """
    im = Image.open(path).convert("RGB")
    W, H = im.size
    px = im.load()
    bg = px[2, 2]
    cols = [x for x in range(W) if any(px[x, y] != bg for y in range(H))]
    rows = [y for y in range(H) if any(px[x, y] != bg for x in range(W))]
    if not cols or not rows:
        raise AssertionError(f"{path}: nothing drawn")
    l, r, t_, b = min(cols), max(cols), min(rows), max(rows)
    worst = min(l, W - 1 - r, t_, H - 1 - b)
    if worst < margin:
        raise AssertionError(
            f"{os.path.basename(path)} breaks GitHub's {margin}px safe border: "
            f"left {l} right {W - 1 - r} top {t_} bottom {H - 1 - b}"
        )
    print(
        f"  safe border OK: left {l} right {W - 1 - r} top {t_} bottom {H - 1 - b} "
        f"(need >= {margin})"
    )


# --- mark: one poll cycle at the measured duty -----------------------------
def render_mark(path, size=512):
    """Low, burst, low - bleeding off both edges.

    The width is one poll period and the burst is 30% of it, which is the
    capture's own BST 12ms of PER 40ms and is also SPECTER_FULL_SCALE_DUTY, the
    duty at which the app's meter reads 100%. One number, two independent
    reasons to be that number.

    Bleeding off both edges is what makes it work small: no margin to lose,
    nothing floating in a box, and it survives a circular avatar crop because
    the lines exit through the circle and read as continuing past it. Small
    sizes are drawn natively rather than downscaled, so nothing anti-aliases
    into grey mush."""
    native = size <= 64
    SS = 1 if native else 3
    S = size * SS

    img = Image.new("RGB", (S, S), BLACK)
    d = ImageDraw.Draw(img)

    stroke = max(1, int(round(68 / 512.0 * S)))
    y_lo = int(round(346 / 512.0 * S))
    y_hi = int(round(166 / 512.0 * S))
    duty = 30 / 100.0  # SPECTER_FULL_SCALE_DUTY, and the capture's 12ms of 40ms
    x_rise = int(round((1.0 - duty) / 2.0 * S))
    x_fall = int(round(((1.0 - duty) / 2.0 + duty) * S))
    half = stroke // 2
    # Order and extent both matter, and getting either wrong is visible at 32px.
    # PIL centres a width-w line on rows y-(w-1)//2 .. y+w//2, so a vertical
    # inset by half a stroke pokes above the bar it meets (the mark grew two
    # tabs); and a bar that stops at the vertical's CENTRE only covers half its
    # width, so the join frays. Verticals therefore span the full swing and the
    # bars are drawn over them, each extended half a stroke past the corner so
    # it caps the vertical completely. The step then reads as one stroke that
    # rises out of the low line and drops back onto it.
    #
    # All of this was found by dumping the 32px and 16px marks as pixel maps,
    # not by looking at them - which is the same lesson as the Site Survey bar.
    d.line([x_rise, y_hi, x_rise, y_lo], fill=_guard(AMBER), width=stroke)
    d.line([x_fall, y_hi, x_fall, y_lo], fill=_guard(AMBER), width=stroke)
    d.line([0, y_lo, x_rise + half, y_lo], fill=_guard(BRASS_DEEP), width=stroke)
    d.line([x_fall - half, y_lo, S, y_lo], fill=_guard(BRASS_DEEP), width=stroke)
    d.line([x_rise - half, y_hi, x_fall + half, y_hi], fill=_guard(AMBER), width=stroke)

    out = img if native else img.resize((size, size), Image.LANCZOS)
    out.save(path)
    print("wrote", path, out.size)


if __name__ == "__main__":
    info = series()
    print(
        f"carrier: {len(info['bits'])} columns, {info['duty']}% duty "
        f"(device printed {info['device_up']}%), {info['edges']} edges, v{info['version']}"
    )
    render_banner(os.path.join(OUT, "banner.png"), "light")
    render_banner(os.path.join(OUT, "banner-dark.png"), "dark")
    render_card(os.path.join(OUT, "social-preview.png"))
    render_mark(os.path.join(OUT, "mark.png"), 512)
    for n in (180, 64):
        Image.open(os.path.join(OUT, "mark.png")).resize((n, n), Image.LANCZOS).save(
            os.path.join(OUT, f"mark-{n}.png")
        )
        print("wrote", os.path.join(OUT, f"mark-{n}.png"), (n, n))
    for n in (32, 16):
        render_mark(os.path.join(OUT, f"mark-{n}.png"), n)

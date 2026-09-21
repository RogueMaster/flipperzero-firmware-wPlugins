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
    "light": dict(ground=PAPER, ink=INK, second=GRAPHITE, rule=HAIRLINE,
                  up=BRASS_DEEP, down=BRASS),
    "dark": dict(ground=BLACK, ink=BONE, second=ASH, rule=SCORE,
                 up=AMBER, down=BRASS_DEEP),
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
        raise AssertionError("LAW 1: EMBER is never drawn - it may only be pasted from a capture")
    return fill


def text(d, x, y, s, f, fill, tracking=0, anchor="ls"):
    """Baseline-anchored text with letter-spacing. Refuses accent colours."""
    if tuple(fill) in RAMP:
        raise AssertionError(f"LAW 2: the accent draws measured carrier, not words ({s!r})")
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
    cap = (Image.open(os.path.join(HERE, "screenshots", name)).convert("RGB")
           .resize((128, 64), Image.NEAREST)  # exact 4x decimation of the 512x256 file
           .resize((128 * scale, 64 * scale), Image.NEAREST))
    out.paste(cap, (int(x), int(y)))
    return cap.size


def series():
    """The recorded carrier, plus the device's own printed duty for cross-check."""
    bits, hi, lo = carrier_from_capture()
    c_hi, c_lo = parse_defines("views/fingerprint_view.c", "TRACE_HI", "TRACE_LO")
    if abs(hi - c_hi) > 1 or abs(lo - c_lo) > 1:
        raise SystemExit(
            f"capture trace rows {hi}/{lo} vs source {c_hi}/{c_lo} - re-shoot screenshots/ss2.png")
    duty = 100.0 * sum(bits) / len(bits)
    edges = sum(1 for i in range(1, len(bits)) if bits[i] != bits[i - 1])
    if edges < 12:
        raise SystemExit(f"only {edges} edges in the capture - not enough polls to show a cadence")
    # UP 27% is printed on that same screen by the firmware's own duty counter.
    device_up = 27
    if abs(duty - device_up) > 4:
        raise SystemExit(f"trace duty {duty:.1f}% has drifted from the device's {device_up}%")
    info = dict(bits=bits, hi=hi, lo=lo, duty=round(duty, 1), edges=edges,
                device_up=device_up, version=version(),
                capture="screenshots/ss2.png",
                capture_mtime=os.path.getmtime(os.path.join(HERE, "screenshots", "ss2.png")))
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
    d.rectangle([u(818), u(94), u(1214), u(298)], outline=_guard(t["rule"]), width=max(1, u(2)))

    text(d, L, u(206), "SPECTER", f_word, t["ink"], u(13))
    text(d, L, u(244), "Sweep for the readers you can't see.", f_tag, t["ink"])
    text(d, L, u(272),
         "Find it · fingerprint it · survey the room · leave it on watch.",
         f_sub, t["second"])

    carrier_band(d, info["bits"], L, R, u(314), u(350), t["up"], t["down"],
                 max(1, u(5)), max(1, u(4)))

    text(d, L, u(372), "github.com/at0m-b0mb/Specter-FlipperZero", f_foot, t["second"], u(2))
    text(d, R, u(372),
         f"POLLING · PER 40 ms · UP {info['device_up']}% DEVICE · "
         f"{round(info['duty'])}% THIS TRACE · v{info['version']}",
         f_foot, t["second"], u(2), anchor="rs")

    out = img.resize((W, H), Image.LANCZOS)  # collapse supersampling FIRST
    scale = int(round(3 * K))  # 3 on the authoring grid, 6 at 2560
    paste_capture(out, "ss2.png", 824 * K, 100 * (H / 400.0), scale)
    assert out.getpixel((int(830 * K), int(110 * (H / 400.0)))) in (EMBER, BLACK), \
        "the device plate was resampled - it must stay two-colour"
    out.save(path)
    print("wrote", path, out.size, f"[{theme}]")


# --- social card: 1280x640, authored 1:1 -----------------------------------
def render_card(path, W=1280, H=640, SS=2):
    t = THEME["dark"]
    info = series()

    def u(v):
        return int(round(v * SS))

    img = Image.new("RGB", (W * SS, H * SS), t["ground"])
    d = ImageDraw.Draw(img)

    f_kick = font(MONO, u(15))
    # Didot BOLD here, and only here: Regular's thins at 104px are about two
    # pixels, which is half a pixel once a feed scales this to ~320px, and they
    # simply vanish. Changing weight to survive the medium is a typographic
    # decision; shipping an illegible thumbnail is not.
    f_word = font(SERIF, u(104), index=2)
    f_tag = font(SANS, u(24))
    f_foot = font(MONO, u(14))

    text(d, u(640), u(48),
         "FLIPPER ZERO · 13.56 MHz · LISTEN-ONLY · NEVER TRANSMITS",
         f_kick, t["second"], u(5), anchor="ms")

    text(d, u(640), u(428), "SPECTER", f_word, t["ink"], u(12), anchor="ms")
    text(d, u(640), u(470), "Sweep for the readers you can't see.", f_tag, t["second"], anchor="ms")

    # full bleed: the same recording as the banner, so both show one moment
    carrier_band(d, info["bits"], 0, u(1280), u(548), u(584), t["up"], t["down"],
                 max(1, u(6)), max(1, u(5)))

    text(d, u(64), u(616), "github.com/at0m-b0mb/Specter-FlipperZero", f_foot, t["second"], u(2))
    text(d, u(1216), u(616), f"v{info['version']} · MIT", f_foot, t["second"], u(2), anchor="rs")

    out = img.resize((W, H), Image.LANCZOS)
    # No mount and no frame: the capture's own ground is already true black, so
    # a frame here would be a box drawn around nothing.
    paste_capture(out, "ss0.png", 384, 68, 4)
    out.save(path)
    print("wrote", path, out.size, "[card]")


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
    print(f"carrier: {len(info['bits'])} columns, {info['duty']}% duty "
          f"(device printed {info['device_up']}%), {info['edges']} edges, v{info['version']}")
    render_banner(os.path.join(OUT, "banner.png"), "light")
    render_banner(os.path.join(OUT, "banner-dark.png"), "dark")
    render_card(os.path.join(OUT, "social-preview.png"))
    render_mark(os.path.join(OUT, "mark.png"), 512)
    for n in (180, 64):
        Image.open(os.path.join(OUT, "mark.png")).resize((n, n), Image.LANCZOS).save(
            os.path.join(OUT, f"mark-{n}.png"))
        print("wrote", os.path.join(OUT, f"mark-{n}.png"), (n, n))
    for n in (32, 16):
        render_mark(os.path.join(OUT, f"mark-{n}.png"), n)

#!/usr/bin/env python3
"""Render Flipper-style mock screenshots (128x64, orange theme) for the README.

These mirror the on-device draw code in views/*.c and the scenes. Text is placed
by BASELINE (anchor "ls"/"rs"/"ms"), because canvas_draw_str() takes y as the
baseline - drawing these any other way quietly hides real layout collisions.
Layout constants below are copied from the C, so if a screen looks wrong here it
looks wrong on the device too.

CAVEATS, both of which have hidden real bugs:
  * VERTICALLY the desktop font sits one pixel shorter than FontSecondary, so
    this will NOT reveal a tight collision - two of those shipped before anyone
    noticed. tools_check_layout.py is the authority on overlaps.
  * HORIZONTALLY the desktop font's natural advance is 4.0 device px per
    character where the device's haxrcorp4089 advances 5. Every string rendered
    here therefore came out 20% narrower than on a Flipper, which is exactly why
    an overrunning readout ("PK100 C999+", 11 chars from x=68) looked fine in
    the published screenshots for several releases. FontSecondary is now drawn
    glyph by glyph at a fixed SEC_PITCH, so a string that runs off the right
    edge of a real screen runs off the right edge here too.
"""
from PIL import Image, ImageDraw, ImageFont
import math, os

S = 6  # scale
W, H = 128, 64
# Sampled, not chosen. Every capture in screenshots/ is exactly two colours,
# (254,138,44) and (0,0,0) - so these are the values the hardware emits, and a
# generated screen and a real one are now the same two colours. They used to be
# (255,130,0) on (10,8,4), which was close enough to look right on its own and
# visibly off the moment the two sat side by side in the README.
BG = (254, 138, 44)  # #FE8A2C - the Flipper's backlight, measured
FG = (0, 0, 0)  # true black, as the panel actually renders it
HERE_DIR = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE_DIR, "images")
os.makedirs(OUT, exist_ok=True)

MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

# --- geometry, copied from the C ------------------------------------------
# views/sweep_view.c
PCX, PCY = 32, 48
R_ARC, R_OUT, R_IN, R_NDL, R_SCN = 26, 26, 22, 23, 19
# views/fingerprint_view.c
FP_CLASS, FP_BLURB, FP_STAT1, FP_STAT2 = 22, 31, 40, 49
FP_COL_R = 66
CONF_X, CONF_Y, CONF_W, CONF_H = 88, 14, 38, 8
FP_DIV, TRACE_HI, TRACE_LO = 51, 54, 62
# views/survey_view.c
BAR_X, BAR_Y, BAR_W, BAR_H = 2, 14, 124, 11
RUN_S1, RUN_S2, RUN_WAVE_TOP, RUN_WAVE_BASE = 35, 45, 50, 63
BANNER_X, BANNER_Y, BANNER_W, BANNER_H = 2, 14, 124, 14
DONE_V, DONE_A, DONE_S1, DONE_S2 = 25, 37, 50, 60
SV_COL_R = 66
# helpers/field_detector.c: TRACE_SLICE_SAMPLES * SAMPLE_PERIOD_US
TRACE_MS_PER_COL = 8


def font(path, px):
    return ImageFont.truetype(path, px)


f_sec = font(MONO, 7 * S - 2)
f_pri = font(BOLD, 8 * S)
f_big = font(BOLD, 19 * S)


def canvas():
    img = Image.new("RGB", (W * S, H * S), BG)
    return img, ImageDraw.Draw(img)


def L(v):
    return int(round(v * S))


def line(d, x0, y0, x1, y1, col=FG, w=2):
    d.line([L(x0), L(y0), L(x1), L(y1)], fill=col, width=w)


def circle(d, cx, cy, r, col=FG, w=2):
    d.ellipse(
        [L(cx) - L(r), L(cy) - L(r), L(cx) + L(r), L(cy) + L(r)], outline=col, width=w
    )


def disc(d, cx, cy, r, col=FG):
    d.ellipse([L(cx) - L(r), L(cy) - L(r), L(cx) + L(r), L(cy) + L(r)], fill=col)


def box(d, x, y, w, h, col=FG):
    d.rectangle([L(x), L(y), L(x + w), L(y + h)], fill=col)


def frame(d, x, y, w, h, col=FG, lw=2):
    d.rectangle([L(x), L(y), L(x + w), L(y + h)], outline=col, width=lw)


def text(d, x, y, s, fnt=f_sec, col=FG, anchor="lm"):
    d.text((L(x), L(y)), s, font=fnt, fill=col, anchor=anchor)


SEC_PITCH = 5  # device advance per FontSecondary glyph (haxrcorp4089)


# The Logbook's TextBox uses a narrower face than FontSecondary: measured off a
# 4x device capture, "  WATCH  contact 2 at 8s fiel" is 29 characters and fills
# the 128px width, i.e. ~4.2px per glyph against FontSecondary's 5.
TEXTBOX_PITCH = 4


def sec(d, x, y, s, col=FG, anchor="ls", pitch=None):
    """FontSecondary at the DEVICE's advance, not the desktop font's, drawn one
    glyph at a time. This is the only way overflow shows up here."""
    SEC_PITCH_LOCAL = SEC_PITCH if pitch is None else pitch
    w = len(s) * SEC_PITCH_LOCAL
    if anchor == "rs":
        x -= w
    elif anchor == "ms":
        x -= w / 2.0
    for i, ch in enumerate(s):
        d.text(
            (L(x + i * SEC_PITCH_LOCAL), L(y)), ch, font=f_sec, fill=col, anchor="ls"
        )


def tb(d, x, y, s, fnt=f_sec, col=FG, anchor="ls"):
    """Baseline-anchored text - y is the baseline, exactly like canvas_draw_str."""
    if fnt is f_sec:
        sec(d, x, y, s, col, anchor)
        return
    d.text((L(x), L(y)), s, font=fnt, fill=col, anchor=anchor)


def dot(d, x, y, col=FG):
    d.rectangle([L(x), L(y), L(x) + S - 1, L(y) + S - 1], fill=col)


def save(img, name):
    p = os.path.join(OUT, name)
    img.save(p)
    print("wrote", p)


def gauge_point(value, radius):
    value = max(0, min(100, value))
    a = math.pi * (1.0 - value / 100.0)
    return PCX + math.cos(a) * radius, PCY - math.sin(a) * radius


def proximity_word(s, saturated=False):
    if saturated:
        return "PEGGED"
    return _proximity_word(s)


def _proximity_word(s):
    return (
        "STRONG" if s >= 70 else "CLOSE" if s >= 45 else "NEAR" if s >= 20 else "FAINT"
    )


# --------------------------------------------------------------------------
# shared chrome
# --------------------------------------------------------------------------
def draw_header(d, title, state, present, flash=None, state_x=116):
    tb(d, 2, 9, title, f_sec)
    if flash:
        box(d, 74, 0, 54, 11)
        tb(d, 125, 9, flash, f_sec, BG, anchor="rs")
    else:
        tb(d, state_x, 9, state, f_sec, anchor="rs")
        if present:
            disc(d, 123, 5, 2)
        else:
            circle(d, 123, 5, 2)
    line(d, 0, 11, 127, 11)


def draw_waveform(d, history, base, span):
    for k in range(62):
        idx = (len(history) - 1 - k) % len(history)
        val = history[idx]
        x = 126 - k * 2
        y = base - (val * span) // 100
        if y < base:
            line(d, x, base, x, y, FG, w=S)
        else:
            dot(d, x, base)


# --------------------------------------------------------------------------
# Sweep (views/sweep_view.c)
# --------------------------------------------------------------------------
# field_scale_apply(threshold + 1, SPECTER_FULL_SCALE_DUTY=30), i.e. the first
# duty that counts as a reader, on the meter's own scale:
#   High (thr 0) -> 3    Medium (thr 8) -> 30    Low (thr 20) -> 70
# A calibrated Custom typically lands in the twenties.
SENS_MARK = {"High": 3, "Medium": 30, "Low": 70, "Custom": 27}


def draw_gauge(d, strength, peak, present, anim, scan=40, threshold=30):
    px = py = None
    v = 0
    while v <= 100:
        ax, ay = gauge_point(v, R_ARC)
        if px is not None:
            line(d, px, py, ax, ay)
        px, py = ax, ay
        v += 3
    for i in range(11):
        vv = i * 10
        ox, oy = gauge_point(vv, R_OUT)
        ix, iy = gauge_point(vv, R_IN)
        line(d, ix, iy, ox, oy)
    # where "READER" begins at the current sensitivity
    mx, my = gauge_point(threshold, R_IN)
    mox, moy = gauge_point(threshold, R_OUT + 2)
    line(d, mx, my, mox, moy)
    line(d, mx + 1, my, mox + 1, moy)
    if not present:
        sx, sy = gauge_point(scan, R_SCN)
        circle(d, sx, sy, 1)
    tx, ty = gauge_point(strength, R_NDL)
    line(d, PCX, PCY, tx, ty)
    line(d, PCX - 1, PCY, tx, ty)
    disc(d, tx, ty, 1)
    kx, ky = gauge_point(peak, R_OUT - 1)
    disc(d, kx, ky, 1)
    disc(d, PCX, PCY, 3)
    if present:
        circle(d, PCX, PCY, R_OUT + 1 + (anim % 3))


def draw_trend(d, x, y, direction):
    """Mirrors draw_trend() in views/sweep_view.c exactly."""
    if direction == 0:
        line(d, x - 2, y + 2, x + 2, y + 2)
        return
    tip = y if direction > 0 else y + 5
    tail = y + 5 if direction > 0 else y
    barb = y + 3 if direction > 0 else y + 2
    line(d, x, tail, x, tip)
    line(d, x - 2, barb, x, tip)
    line(d, x + 2, barb, x, tip)


def draw_readout(d, strength, peak, contacts, trend=None):
    line(d, 64, 12, 64, 51)
    tb(d, 68, 20, "FIELD", f_sec)
    if trend is not None:
        draw_trend(d, 120, 14, trend)
    tb(d, 112, 42, str(strength), f_big, anchor="rs")
    tb(d, 114, 40, "%", f_sec)
    tb(d, 68, 51, f"PEAK {peak}%", f_sec)


def render_sweep(
    name,
    strength,
    peak,
    contacts,
    present,
    state,
    history,
    anim=1,
    calibrating=False,
    calib_pct=0,
    flash=None,
    sens="Medium",
    saturated=False,
    trend=None,
):
    img, d = canvas()
    draw_header(d, "SWEEP", state, present, flash)
    draw_gauge(d, strength, peak, present, anim, threshold=SENS_MARK.get(sens, 30))
    draw_readout(d, strength, peak, contacts, trend)
    line(d, 0, 52, 127, 52)
    if calibrating:
        tb(d, 2, 62, "HOLD STILL", f_sec)
        tb(d, 126, 62, "OK=cancel", f_sec, anchor="rs")
        fill = (calib_pct * 128) // 100
        if fill:
            box(d, 0, 63, fill, 1)
    elif present:
        box(d, 0, 53, 128, 11)
        disc(d, 4, 58, 1, BG)
        tb(d, 9, 62, "ACTIVE READER", f_sec, BG)
        tb(d, 125, 62, proximity_word(strength, saturated), f_sec, BG, anchor="rs")
        frame(d, 0, 0, 127, 63, FG, lw=2)
    elif not contacts:
        # nothing found yet: the strip carries the two undiscoverable keys
        tb(d, 2, 62, "LEFT=cal hold OK=log", f_sec)
    else:
        # active sensitivity on the left, waveform filling the rest
        label = f"SENS {sens}"
        tb(d, 2, 62, label, f_sec)
        wave_left = 2 + len(label) * SEC_PITCH + 4
        for k in range(62):
            x = 126 - k * 2
            if x < wave_left:
                break
            idx = (len(history) - 1 - k) % len(history)
            v = history[idx]
            y = 63 - (v * 9) // 100
            if y < 63:
                line(d, x, 63, x, y, FG, w=S)
            else:
                dot(d, x, 63)
    save(img, name)


# --------------------------------------------------------------------------
# Fingerprint (views/fingerprint_view.c)
# --------------------------------------------------------------------------
def pulse_train(period_ms, burst_ms, columns=W):
    """Reproduce what the detector's trace buffer would hold for this cadence."""
    per = max(1, round(period_ms / TRACE_MS_PER_COL))
    bst = max(1, round(burst_ms / TRACE_MS_PER_COL))
    return [1 if (i % per) < bst else 0 for i in range(columns)]


def draw_trace(d, bits):
    prev = None
    for i, hi in enumerate(bits):
        y = TRACE_HI if hi else TRACE_LO
        dot(d, i, y)
        if prev is not None and hi != prev:
            line(d, i, TRACE_HI, i, TRACE_LO)
        prev = hi


def render_fingerprint(
    name,
    klass,
    blurb,
    conf,
    period,
    burst,
    jitter,
    duty,
    present=True,
    state="LISTENING",
    approx="",
    flash=None,
    has_cadence=True,
):
    img, d = canvas()
    draw_header(d, "FINGERPRINT", state, present, flash)

    tb(d, 2, FP_CLASS, klass, f_pri)
    frame(d, CONF_X, CONF_Y, CONF_W, CONF_H)
    fill = (conf * (CONF_W - 2)) // 100
    if fill:
        box(d, CONF_X + 1, CONF_Y + 1, fill, CONF_H - 2)

    tb(d, 2, FP_BLURB, blurb, f_sec)
    tb(d, 126, FP_BLURB, f"CONF {conf}%", f_sec, anchor="rs")

    if has_cadence:
        tb(d, 2, FP_STAT1, f"PER {approx}{period}ms", f_sec)
        tb(d, FP_COL_R, FP_STAT1, f"BST {approx}{burst}ms", f_sec)
        tb(d, 2, FP_STAT2, f"JIT {approx}{jitter}ms", f_sec)
    else:
        tb(d, 2, FP_STAT1, "PER --", f_sec)
        tb(d, FP_COL_R, FP_STAT1, "BST --", f_sec)
        tb(d, 2, FP_STAT2, "JIT --", f_sec)
    tb(d, FP_COL_R, FP_STAT2, f"UP {duty}%", f_sec)

    line(d, 0, FP_DIV, 127, FP_DIV)
    draw_trace(d, pulse_train(period, burst) if has_cadence else [0] * W)
    save(img, name)


# --------------------------------------------------------------------------
# Site Survey (views/survey_view.c)
# --------------------------------------------------------------------------
def render_survey_running(name, pct, left_s, field, peak, hits, present, history):
    img, d = canvas()
    mins, secs = divmod(left_s, 60)
    draw_header(d, "SITE SURVEY", f"{mins}:{secs:02d}", present)

    frame(d, BAR_X, BAR_Y, BAR_W, BAR_H)
    fill = (pct * (BAR_W - 2)) // 100
    if fill:
        box(d, BAR_X + 1, BAR_Y + 1, fill, BAR_H - 2)

    tb(d, 2, RUN_S1, f"FIELD {field}%", f_sec)
    tb(d, SV_COL_R, RUN_S1, f"PEAK {peak}%", f_sec)
    tb(d, 2, RUN_S2, f"HITS {hits}", f_sec)
    tb(d, SV_COL_R, RUN_S2, "OK=finish", f_sec)

    line(d, 0, RUN_WAVE_TOP - 2, 127, RUN_WAVE_TOP - 2)
    draw_waveform(d, history, RUN_WAVE_BASE, RUN_WAVE_BASE - RUN_WAVE_TOP)
    save(img, name)


def render_survey_verdict(name, verdict, advice, mx, av, field, hits, secs=60):
    img, d = canvas()
    alarm = verdict == "ACTIVE READER"
    # the finished card names the run's real length and drops the presence dot
    tb(d, 2, 9, f"SURVEY {secs}s", f_sec)
    tb(d, 116, 9, "OK=again", f_sec, anchor="rs")
    line(d, 0, 11, 127, 11)

    if alarm:
        box(d, BANNER_X, BANNER_Y, BANNER_W, BANNER_H)
    else:
        frame(d, BANNER_X, BANNER_Y, BANNER_W, BANNER_H)
    tb(d, 64, DONE_V, verdict, f_pri, BG if alarm else FG, anchor="ms")

    tb(d, 64, DONE_A, advice, f_sec, anchor="ms")
    line(d, 0, DONE_A + 3, 127, DONE_A + 3)

    tb(d, 2, DONE_S1, f"PEAK {mx}%", f_sec)
    tb(d, SV_COL_R, DONE_S1, f"AVG {av}%", f_sec)
    tb(d, 2, DONE_S2, f"HITS {hits}", f_sec)
    tb(d, SV_COL_R, DONE_S2, f"UP {field}%", f_sec)

    if alarm:
        frame(d, 0, 0, 127, 63, FG, lw=2)
    save(img, name)


# --------------------------------------------------------------------------
# Menus, settings, logbook
# --------------------------------------------------------------------------
def render_menu():
    img, d = canvas()
    tb(d, 4, 11, "Specter", f_pri)
    line(d, 0, 14, 127, 14)
    items = [
        "Sweep - find it",
        "Fingerprint - type",
        "Site Survey - room",
        "Watch Mode - guard",
    ]
    ROW_H = 12
    for i, it in enumerate(items):
        y = 15 + i * ROW_H
        col = FG
        if i == 0:
            box(d, 0, y, 124, ROW_H)
            col = BG
        tb(d, 6, y + 9, it, f_sec, col)
    box(d, 125, 15, 3, 24)
    save(img, "screen_menu.png")


def render_settings():
    img, d = canvas()
    rows = [
        ("Sensitivity", "Custom", True),
        ("Survey time", "60s", False),
        ("Sound", "ON", False),
        ("Vibrate", "ON", False),
        ("LED", "ON", False),
    ]
    ROW_H = 12
    for i, (k, v, sel) in enumerate(rows):
        y = 2 + i * ROW_H
        col = FG
        if sel:
            box(d, 0, y, 124, ROW_H)
            col = BG
        tb(d, 4, y + 9, k, f_sec, col)
        tb(d, 121, y + 9, v, f_sec, col, anchor="rs")
    box(d, 125, 2, 3, 22)
    save(img, "screen_settings.png")


def wrapped_entries(entries):
    """Lay entries out using the app's OWN wrapper (helpers/log_wrap.h).

    This screenshot used to show invented one-line entries like
    "WATCH  hit 4 @92s f61%" that the app has never written - the real details
    are three times that long and wrap. Compiling the actual header means the
    picture cannot drift from the device again.
    """
    import subprocess, tempfile, json

    prog = [
        '#include "log_wrap.h"',
        "#include <stdio.h>",
        "int main(void){",
        "  char b[512];",
    ]
    for t, d in entries:
        esc = d.replace("\\", "\\\\").replace('"', '\\"')
        prog.append(
            f'  specter_log_wrap(b, sizeof b, "{t}", "{esc}"); fputs(b, stdout);'
        )
    prog.append("  return 0;\n}")

    with tempfile.TemporaryDirectory() as td:
        c = os.path.join(td, "lb.c")
        exe = os.path.join(td, "lb")
        open(c, "w").write("\n".join(prog))
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-I",
                os.path.join(HERE_DIR, "helpers"),
                "-o",
                exe,
                c,
            ],
            check=True,
        )
        out = subprocess.run([exe], check=True, capture_output=True, text=True).stdout
    return out.rstrip("\n").split("\n")


def render_logbook():
    """The on-device viewer: a timestamp line, then the detail on one or more
    indented lines exactly as helpers/log_wrap.h lays it out."""
    img, d = canvas()
    entries = [
        (
            "2026-09-21 12:19:28",
            ("WATCH", "contact 2 at 8s field 17% peak 100% m:boost"),
        ),
        (
            "2026-09-21 12:18:03",
            ("SURVEY", "60s ACTIVE READER peak 74% avg 21% infield 38% hits 5 m:boost"),
        ),
    ]
    lines = []
    for stamp, (t, detail) in entries:
        lines.append(stamp)
        lines.extend(wrapped_entries([(t, detail)]))

    for i, s in enumerate(lines[:7]):
        sec(d, 2, 9 + i * 8, s, pitch=TEXTBOX_PITCH)
    box(d, 125, 14, 3, 34)  # scrollbar, parked near the end
    save(img, "screen_logbook.png")


# --------------------------------------------------------------------------
# Watch Mode (views/watch_view.c)
# --------------------------------------------------------------------------
def render_watch(
    name,
    watching_s,
    contacts,
    peak,
    present,
    strength=0,
    last_ago="--",
    blink=True,
    seen_s=0,
):
    img, d = canvas()
    draw_header(d, "WATCH", "READER" if present else "LISTENING", present)

    STATUS_Y, STATUS_H, CLOCK_BASE = 14, 14, 34
    FOOT1, FOOT2, COLR = 50, 60, 66

    if present:
        # Solid, never strobing - an inverted block is already the loudest thing
        # on a 1-bit screen. (The pair of "liveness" discs this used to draw did
        # not exist in watch_view.c at all.)
        box(d, 0, STATUS_Y - 1, 128, STATUS_H)
        tb(d, 64, STATUS_Y + 9, "ACTIVE READER", f_pri, BG, anchor="ms")
        # how strong, readable from across a room
        frame(d, 4, 29, 120, 9)
        w = (strength * 118) // 100
        if w > 0:
            box(d, 5, 30, w, 7)
        frame(d, 0, 0, 127, 63, FG, lw=2)
    else:
        word = "QUIET NOW" if contacts else "NO READER"
        tb(d, 4, STATUS_Y + 9, word, f_pri, anchor="ls")
        mm, ss = divmod(watching_s, 60)
        hours = watching_s > 99 * 60 + 59
        if hours:
            mm, ss = watching_s // 3600, (watching_s // 60) % 60
        tb(
            d,
            120 if hours else 126,
            CLOCK_BASE,
            f"{mm:02d}:{ss:02d}",
            f_big,
            anchor="rs",
        )
        if hours:
            tb(d, 121, CLOCK_BASE, "h", f_sec)
        if contacts:
            tb(d, 4, 33, "OK=re-arm", f_sec)

    line(d, 0, FOOT1 - 10, 127, FOOT1 - 10)
    tb(d, 2, FOOT1, f"HITS {contacts}", f_sec)
    tb(d, COLR, FOOT1, f"PEAK {peak}%", f_sec)
    tb(d, 2, FOOT2, f"LAST {last_ago}", f_sec)
    if present:
        tb(d, COLR, FOOT2, f"NOW {strength}%", f_sec)
    elif contacts:
        tb(d, COLR, FOOT2, f"UP {seen_s}s", f_sec)
    else:
        tb(d, COLR, FOOT2, "OK=re-arm", f_sec)
    save(img, name)


CLEAR_HIST = [
    3,
    5,
    2,
    8,
    4,
    1,
    6,
    3,
    9,
    5,
    2,
    7,
    4,
    11,
    6,
    3,
    8,
    5,
    2,
    10,
    6,
    4,
    9,
    5,
    3,
    7,
    12,
    6,
    4,
    8,
    5,
    14,
    7,
    4,
    9,
    6,
    3,
    8,
    5,
    11,
    6,
    4,
    7,
    3,
    9,
    5,
    2,
    8,
    13,
    6,
    4,
    7,
    5,
    10,
    6,
    3,
    8,
    5,
    2,
    7,
    4,
    9,
]

SURVEY_HIST = [
    4,
    6,
    3,
    9,
    5,
    2,
    7,
    12,
    22,
    38,
    51,
    44,
    30,
    18,
    9,
    5,
    3,
    8,
    4,
    6,
    11,
    7,
    4,
    9,
    5,
    3,
    8,
    15,
    28,
    41,
    36,
    24,
    13,
    7,
    4,
    9,
    5,
    2,
    8,
    6,
    3,
    10,
    5,
    7,
    4,
    9,
    6,
    3,
    8,
    5,
    12,
    7,
    4,
    10,
    6,
    3,
    9,
    5,
    8,
    4,
    7,
    3,
]


def strip(names, out, cols=None):
    imgs = [Image.open(os.path.join(OUT, n)) for n in names]
    cols = cols or len(imgs)
    rows = (len(imgs) + cols - 1) // cols
    pad = 18
    cw, ch = imgs[0].width, imgs[0].height
    sheet = Image.new(
        "RGB",
        (cw * cols + pad * (cols + 1), ch * rows + pad * (rows + 1)),
        (12, 14, 20),
    )
    for i, im in enumerate(imgs):
        r, c = divmod(i, cols)
        sheet.paste(im, (pad + c * (cw + pad), pad + r * (ch + pad)))
    sheet.save(os.path.join(OUT, out))
    print("wrote", os.path.join(OUT, out))


if __name__ == "__main__":
    # Nothing found yet - what a first-time user actually sees, hint and all.
    render_sweep(
        "screen_clear.png", 7, 18, 0, False, "LISTENING", CLEAR_HIST, anim=2, trend=0
    )
    # Same quiet room, but after a contact: sensitivity and the live waveform.
    render_sweep(
        "screen_quiet.png", 9, 22, 1, False, "LISTENING", CLEAR_HIST, anim=2, trend=-1
    )
    # A real polling reader at arm's length, and the Flipper laid on top of one
    # (raw duty ~31% saturates the meter -> reads MAX, not "31%").
    render_sweep(
        "screen_reader.png", 78, 86, 3, True, "READER", CLEAR_HIST, anim=1, trend=1
    )
    render_sweep(
        "screen_reader_max.png",
        100,
        100,
        4,
        True,
        "READER",
        CLEAR_HIST,
        anim=1,
        saturated=True,
        trend=1,
    )
    render_sweep(
        "screen_calibrate.png",
        4,
        9,
        0,
        False,
        "CALIBRATING",
        CLEAR_HIST,
        anim=2,
        calibrating=True,
        calib_pct=62,
        sens="Custom",
    )

    render_fingerprint(
        "screen_fingerprint.png", "POLLING", "Fixed poll", 88, 204, 24, 2, 11
    )
    render_fingerprint(
        "screen_fingerprint_cw.png",
        "CONTINUOUS",
        "Always on",
        100,
        0,
        0,
        0,
        98,
        has_cadence=False,
    )

    render_survey_running("screen_survey_run.png", 62, 23, 9, 51, 2, False, SURVEY_HIST)
    render_survey_verdict(
        "screen_survey_done.png", "ACTIVE READER", "Fingerprint it", 74, 21, 38, 5
    )
    render_survey_verdict(
        "screen_survey_clean.png", "CLEAN", "No field detected", 6, 2, 0, 0
    )

    render_watch(
        "screen_watch.png",
        watching_s=752,
        contacts=2,
        peak=100,
        present=False,
        last_ago="3m12s",
        seen_s=47,
    )
    render_watch(
        "screen_watch_hit.png",
        watching_s=92,
        contacts=4,
        peak=71,
        present=True,
        strength=63,
        last_ago="0s",
    )

    render_menu()
    render_settings()
    render_logbook()

    strip(
        (
            "screen_reader.png",
            "screen_fingerprint.png",
            "screen_survey_done.png",
            "screen_watch_hit.png",
        ),
        "screens.png",
    )
    strip(
        (
            "screen_clear.png",
            "screen_quiet.png",
            "screen_reader.png",
            "screen_fingerprint.png",
            "screen_survey_run.png",
            "screen_survey_done.png",
            "screen_watch.png",
            "screen_watch_hit.png",
            "screen_logbook.png",
            "screen_calibrate.png",
        ),
        "screens_all.png",
        cols=5,
    )

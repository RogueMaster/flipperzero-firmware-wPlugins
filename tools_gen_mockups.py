#!/usr/bin/env python3
"""Render Flipper-style mock screenshots (128x64, orange theme) for the README.

These mirror the on-device draw code in views/*.c and the scenes. Text is placed
by BASELINE (anchor "ls"/"rs"/"ms"), because canvas_draw_str() takes y as the
baseline - drawing these any other way quietly hides real layout collisions.
Layout constants below are copied from the C, so if a screen looks wrong here it
looks wrong on the device too.
"""
from PIL import Image, ImageDraw, ImageFont
import math, os

S = 6  # scale
W, H = 128, 64
BG = (255, 130, 0)  # flipper backlight orange
FG = (10, 8, 4)  # near-black pixels
OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

# --- geometry, copied from the C ------------------------------------------
# views/sweep_view.c
PCX, PCY = 32, 48
R_ARC, R_OUT, R_IN, R_NDL, R_SCN = 26, 26, 22, 23, 19
# views/fingerprint_view.c
FP_CLASS, FP_BLURB, FP_STAT1, FP_STAT2 = 22, 31, 40, 48
FP_COL_R = 66
CONF_X, CONF_Y, CONF_W, CONF_H = 88, 15, 38, 8
FP_DIV, TRACE_HI, TRACE_LO = 50, 53, 61
# views/survey_view.c
BAR_X, BAR_Y, BAR_W, BAR_H = 4, 15, 120, 11
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
    d.ellipse([L(cx) - L(r), L(cy) - L(r), L(cx) + L(r), L(cy) + L(r)], outline=col, width=w)


def disc(d, cx, cy, r, col=FG):
    d.ellipse([L(cx) - L(r), L(cy) - L(r), L(cx) + L(r), L(cy) + L(r)], fill=col)


def box(d, x, y, w, h, col=FG):
    d.rectangle([L(x), L(y), L(x + w), L(y + h)], fill=col)


def frame(d, x, y, w, h, col=FG, lw=2):
    d.rectangle([L(x), L(y), L(x + w), L(y + h)], outline=col, width=lw)


def text(d, x, y, s, fnt=f_sec, col=FG, anchor="lm"):
    d.text((L(x), L(y)), s, font=fnt, fill=col, anchor=anchor)


def tb(d, x, y, s, fnt=f_sec, col=FG, anchor="ls"):
    """Baseline-anchored text - y is the baseline, exactly like canvas_draw_str."""
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
        return "MAX"
    return _proximity_word(s)


def _proximity_word(s):
    return "STRONG" if s >= 70 else "CLOSE" if s >= 45 else "NEAR" if s >= 20 else "FAINT"


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
def draw_gauge(d, strength, peak, present, anim, scan=40):
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
        hot = i >= 8
        ox, oy = gauge_point(vv, R_OUT)
        ix, iy = gauge_point(vv, (R_IN - 3) if hot else R_IN)
        line(d, ix, iy, ox, oy)
        if hot:
            line(d, ix + 1, iy, ox + 1, oy)
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
    line(d, 64, 13, 64, 51)
    tb(d, 68, 20, "FIELD", f_sec)
    if trend is not None:
        draw_trend(d, 120, 14, trend)
    tb(d, 112, 42, str(strength), f_big, anchor="rs")
    tb(d, 114, 40, "%", f_sec)
    label = f"PK{peak} C999+" if contacts > 999 else f"PK{peak} C{contacts}"
    tb(d, 68, 51, label, f_sec)


def render_sweep(name, strength, peak, contacts, present, state, history,
                 anim=1, calibrating=False, calib_pct=0, flash=None, sens="Medium",
                 saturated=False, trend=None):
    img, d = canvas()
    draw_header(d, "SPECTER", state, present, flash)
    draw_gauge(d, strength, peak, present, anim)
    draw_readout(d, strength, peak, contacts, trend)
    line(d, 0, 52, 127, 52)
    if calibrating:
        tb(d, 2, 59, "NOISE FLOOR", f_sec)
        tb(d, 126, 59, "OK=cancel", f_sec, anchor="rs")
        frame(d, 0, 60, 128, 4)
        fill = (calib_pct * 126) // 100
        if fill:
            box(d, 1, 61, fill, 2)
    elif present:
        box(d, 0, 53, 128, 11)
        disc(d, 4, 58, 1, BG)
        tb(d, 9, 62, "ACTIVE READER", f_sec, BG)
        tb(d, 125, 62, proximity_word(strength, saturated), f_sec, BG, anchor="rs")
        frame(d, 0, 0, 127, 63, FG, lw=2)
    else:
        # active sensitivity on the left, waveform filling the rest
        label = f"S:{sens}"
        tb(d, 2, 62, label, f_sec)
        wave_left = 2 + int(d.textlength(label, font=f_sec) / S) + 4
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


def render_fingerprint(name, klass, blurb, conf, period, burst, jitter, duty,
                       present=True, state="LISTENING", approx="", flash=None,
                       has_cadence=True):
    img, d = canvas()
    draw_header(d, "FINGERPRINT", state, present, flash)

    tb(d, 2, FP_CLASS, klass, f_pri)
    frame(d, CONF_X, CONF_Y, CONF_W, CONF_H)
    fill = (conf * (CONF_W - 2)) // 100
    if fill:
        box(d, CONF_X + 1, CONF_Y + 1, fill, CONF_H - 2)

    tb(d, 2, FP_BLURB, blurb, f_sec)
    tb(d, 126, FP_BLURB, f"{conf}%", f_sec, anchor="rs")

    if has_cadence:
        tb(d, 2, FP_STAT1, f"PER {approx}{period}ms", f_sec)
        tb(d, FP_COL_R, FP_STAT1, f"BST {approx}{burst}ms", f_sec)
        tb(d, 2, FP_STAT2, f"JIT {approx}{jitter}ms", f_sec)
    else:
        tb(d, 2, FP_STAT1, "PER --", f_sec)
        tb(d, FP_COL_R, FP_STAT1, "BST --", f_sec)
        tb(d, 2, FP_STAT2, "JIT --", f_sec)
    tb(d, FP_COL_R, FP_STAT2, f"DUTY {duty}%", f_sec)

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
    tb(d, SV_COL_R, RUN_S2, "sweep slowly", f_sec)

    line(d, 0, RUN_WAVE_TOP - 2, 127, RUN_WAVE_TOP - 2)
    draw_waveform(d, history, RUN_WAVE_BASE, RUN_WAVE_BASE - RUN_WAVE_TOP)
    save(img, name)


def render_survey_verdict(name, verdict, advice, mx, av, field, hits):
    img, d = canvas()
    alarm = verdict == "ACTIVE READER"
    draw_header(d, "SITE SURVEY", "OK=again", False, state_x=126)
    # the header dot is not drawn in the finished state
    box(d, 118, 0, 10, 10, BG)
    tb(d, 126, 9, "OK=again", f_sec, anchor="rs")

    if alarm:
        box(d, BANNER_X, BANNER_Y, BANNER_W, BANNER_H)
    else:
        frame(d, BANNER_X, BANNER_Y, BANNER_W, BANNER_H)
    tb(d, 64, DONE_V, verdict, f_pri, BG if alarm else FG, anchor="ms")

    tb(d, 64, DONE_A, advice, f_sec, anchor="ms")
    line(d, 0, DONE_A + 3, 127, DONE_A + 3)

    tb(d, 2, DONE_S1, f"MAX {mx}%", f_sec)
    tb(d, SV_COL_R, DONE_S1, f"AVG {av}%", f_sec)
    tb(d, 2, DONE_S2, f"HITS {hits}", f_sec)
    tb(d, SV_COL_R, DONE_S2, f"FIELD {field}%", f_sec)

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
    items = ["Sweep", "Fingerprint", "Site Survey", "Logbook"]
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
    rows = [("Sensitivity", "Custom", True), ("Survey time", "60 s", False),
            ("Sound", "ON", False), ("Vibrate", "ON", False),
            ("Stealth", "ON", False)]
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


def render_logbook():
    """The on-device viewer: each entry is a timestamp line then an indented
    'TYPE detail' line (long details wrap in the real text box; shown here as a
    representative prefix)."""
    img, d = canvas()
    lines = [
        "2026-07-18 14:36:20",
        "  WATCH  hit 4 @92s f61%",
        "2026-07-18 14:35:11",
        "  SURVEY 60s ACTIVE mx74",
        "2026-07-18 14:32:07",
        "  READER POLLING 204ms",
        "2026-07-18 14:30:55",
        "  SWEEP  field 78% pk86%",
    ]
    for i, s in enumerate(lines):
        tb(d, 2, 9 + i * 8, s, f_sec)
    box(d, 125, 14, 3, 34)  # scrollbar, parked near the end
    save(img, "screen_logbook.png")


# --------------------------------------------------------------------------
# Watch Mode (views/watch_view.c)
# --------------------------------------------------------------------------
def render_watch(name, watching_s, contacts, peak, present, strength=0,
                 last_ago="--", blink=True, seen_s=0):
    img, d = canvas()
    tb(d, 2, 9, "WATCH", f_sec)
    tb(d, 126, 9, "ARMED", f_sec, anchor="rs")
    line(d, 0, 11, 127, 11)

    STATUS_Y, STATUS_H, CLOCK_BASE = 14, 14, 34
    FOOT1, FOOT2, COLR = 50, 61, 66

    if present:
        if blink:
            box(d, 0, STATUS_Y - 1, 128, STATUS_H)
            tb(d, 64, STATUS_Y + 9, "READER PRESENT", f_pri, BG, anchor="ms")
        else:
            frame(d, 0, STATUS_Y - 1, 128, STATUS_H)
            tb(d, 64, STATUS_Y + 9, "READER PRESENT", f_pri, FG, anchor="ms")
    else:
        word = "CLEAR NOW" if contacts else "ALL CLEAR"
        tb(d, 4, STATUS_Y + 9, word, f_pri, anchor="ls")
        mm, ss = divmod(watching_s, 60)
        tb(d, 126, CLOCK_BASE, f"{mm:02d}:{ss:02d}", f_big, anchor="rs")

    line(d, 0, FOOT1 - 10, 127, FOOT1 - 10)
    tb(d, 2, FOOT1, f"HITS {contacts}", f_sec)
    tb(d, COLR, FOOT1, f"PEAK {peak}%", f_sec)
    tb(d, 2, FOOT2, f"LAST {last_ago}", f_sec)
    if present:
        tb(d, COLR, FOOT2, f"NOW {strength}%", f_sec)
    elif contacts:
        tb(d, COLR, FOOT2, f"SEEN {seen_s}s", f_sec)
    else:
        tb(d, COLR, FOOT2, "OK=reset", f_sec)
    save(img, name)


CLEAR_HIST = [3, 5, 2, 8, 4, 1, 6, 3, 9, 5, 2, 7, 4, 11, 6, 3, 8, 5, 2, 10, 6, 4, 9,
              5, 3, 7, 12, 6, 4, 8, 5, 14, 7, 4, 9, 6, 3, 8, 5, 11, 6, 4, 7, 3, 9, 5,
              2, 8, 13, 6, 4, 7, 5, 10, 6, 3, 8, 5, 2, 7, 4, 9]

SURVEY_HIST = [4, 6, 3, 9, 5, 2, 7, 12, 22, 38, 51, 44, 30, 18, 9, 5, 3, 8, 4, 6, 11,
               7, 4, 9, 5, 3, 8, 15, 28, 41, 36, 24, 13, 7, 4, 9, 5, 2, 8, 6, 3, 10,
               5, 7, 4, 9, 6, 3, 8, 5, 12, 7, 4, 10, 6, 3, 9, 5, 8, 4, 7, 3]


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
    render_sweep("screen_clear.png", 7, 18, 0, False, "SCANNING", CLEAR_HIST, anim=2,
                 trend=0)
    # A real polling reader at arm's length, and the Flipper laid on top of one
    # (raw duty ~31% saturates the meter -> reads MAX, not "31%").
    render_sweep("screen_reader.png", 78, 86, 3, True, "READER", CLEAR_HIST, anim=1,
                 trend=1)
    render_sweep("screen_reader_max.png", 100, 100, 4, True, "READER", CLEAR_HIST,
                 anim=1, saturated=True, trend=1)
    render_sweep("screen_calibrate.png", 4, 9, 0, False, "CALIBRATE", CLEAR_HIST,
                 anim=2, calibrating=True, calib_pct=62)

    render_fingerprint("screen_fingerprint.png", "POLLING", "Fixed poll cycle",
                       88, 204, 24, 2, 11)
    render_fingerprint("screen_fingerprint_cw.png", "CONTINUOUS", "Carrier held up",
                       100, 0, 0, 0, 98, has_cadence=False)

    render_survey_running("screen_survey_run.png", 62, 23, 9, 51, 2, False, SURVEY_HIST)
    render_survey_verdict("screen_survey_done.png", "ACTIVE READER", "Fingerprint it",
                          74, 21, 38, 5)
    render_survey_verdict("screen_survey_clean.png", "CLEAN", "Nothing emitting here",
                          6, 2, 0, 0)

    render_watch("screen_watch.png", watching_s=752, contacts=2, peak=100, present=False,
                 last_ago="3m12s", seen_s=47)
    render_watch("screen_watch_hit.png", watching_s=92, contacts=4, peak=71, present=True,
                 strength=63, last_ago="0s")

    render_menu()
    render_settings()
    render_logbook()

    strip(("screen_reader.png", "screen_fingerprint.png",
           "screen_survey_done.png", "screen_watch_hit.png"), "screens.png")
    strip(("screen_clear.png", "screen_reader.png", "screen_fingerprint.png",
           "screen_survey_run.png", "screen_survey_done.png", "screen_watch.png",
           "screen_watch_hit.png", "screen_logbook.png", "screen_calibrate.png",
           "screen_settings.png"), "screens_all.png", cols=5)

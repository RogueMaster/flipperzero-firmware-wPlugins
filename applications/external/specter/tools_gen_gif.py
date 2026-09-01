#!/usr/bin/env python3
"""Render the animated demo GIF for the README.

Every value on screen comes from the app's own C, not from numbers chosen to
look good: tools_gif_data.c links the real helpers - the same smoother, the same
presence latch, the same meter scaling, the same proximity vocabulary, the same
trend rule, the same classifier and survey verdict - and prints what the device
would display for a plausible physical trajectory (walk up to a terminal, rest
on it, walk away).

The first version of this animation was drawn by hand and was quietly
impossible: it showed a field of 81% while still claiming to be SCANNING with
zero contacts, when in reality anything over the noise floor latches presence
and flips the screen to the alarm strip straight away.

Drawing comes from tools_gen_mockups, which carries the views' own layout
constants. So the GIF is 1:1 with the app in both what it shows and how.

    python3 tools_gen_gif.py
"""
import os
import subprocess
import tempfile

import tools_gen_mockups as m

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "images")
FRAME_MS = 100  # the app's own UI tick
SENS = "Custom"  # what the sweep strip shows after a calibration


def engine_frames():
    """Compile and run the real logic, and parse what it says to display."""
    src = os.path.join(HERE, "tools_gif_data.c")
    helpers = [
        os.path.join(HERE, "helpers", f)
        for f in ("field_scale.c", "emitter_classify.c", "survey_verdict.c")
    ]
    with tempfile.TemporaryDirectory() as td:
        exe = os.path.join(td, "gifdata")
        subprocess.run(
            ["cc", "-std=c11", "-Wall", "-Wextra", "-I", os.path.join(HERE, "helpers"),
             "-o", exe, src] + helpers,
            check=True,
        )
        out = subprocess.run([exe], check=True, capture_output=True, text=True).stdout

    rows, fp, sv = [], None, None
    for line in out.splitlines():
        if line.startswith("#") or not line.strip():
            continue
        if line.startswith("FP "):
            fp = line[3:].split("|")
        elif line.startswith("SV "):
            sv = line[3:].split("|")
        else:
            raw, ema, shown, peak, present, sat, contacts, trend, word = line.split()
            rows.append(dict(
                raw=int(raw), ema=int(ema), shown=int(shown), peak=int(peak),
                present=bool(int(present)), sat=bool(int(sat)),
                contacts=int(contacts), trend=int(trend), word=word,
            ))
    return rows, fp, sv


def sweep_frame(r, hist, anim):
    img, d = m.canvas()
    state = "READER" if r["present"] else "SCANNING"
    m.draw_header(d, "SPECTER", state, r["present"])
    m.draw_gauge(d, r["shown"], r["peak"], r["present"], anim)
    m.draw_readout(d, r["shown"], r["peak"], r["contacts"], r["trend"])
    m.line(d, 0, 52, 127, 52)
    if r["present"]:
        m.box(d, 0, 53, 128, 11)
        m.disc(d, 4, 58, 1, m.BG)
        m.tb(d, 9, 61, "ACTIVE READER", m.f_sec, m.BG)
        m.tb(d, 125, 61, r["word"], m.f_sec, m.BG, anchor="rs")
        m.frame(d, 0, 0, 127, 63, m.FG, lw=2)
    else:
        label = f"S:{SENS}"
        m.tb(d, 2, 62, label, m.f_sec)
        wave_left = 2 + int(d.textlength(label, font=m.f_sec) / m.S) + 4
        for k in range(62):
            x = 126 - k * 2
            if x < wave_left:
                break
            v = hist[-1 - k] if k < len(hist) else 0
            y = 63 - (v * 9) // 100
            if y < 63:
                m.line(d, x, 63, x, y, m.FG, w=m.S)
            else:
                m.dot(d, x, 63)
    return img


def fingerprint_frame(fp, conf, shift):
    klass, blurb, _c, period, burst, jitter, duty, reliable = fp
    period, burst, jitter, duty = int(period), int(burst), int(jitter), int(duty)
    approx = "" if int(reliable) else "~"
    img, d = m.canvas()
    m.draw_header(d, "FINGERPRINT", "LISTENING", True)
    m.tb(d, 2, m.FP_CLASS, klass, m.f_pri)
    m.frame(d, m.CONF_X, m.CONF_Y, m.CONF_W, m.CONF_H)
    fill = (conf * (m.CONF_W - 2)) // 100
    if fill:
        m.box(d, m.CONF_X + 1, m.CONF_Y + 1, fill, m.CONF_H - 2)
    m.tb(d, 2, m.FP_BLURB, blurb, m.f_sec)
    m.tb(d, 126, m.FP_BLURB, f"{conf}%", m.f_sec, anchor="rs")
    m.tb(d, 2, m.FP_STAT1, f"PER {approx}{period}ms", m.f_sec)
    m.tb(d, m.FP_COL_R, m.FP_STAT1, f"BST {approx}{burst}ms", m.f_sec)
    m.tb(d, 2, m.FP_STAT2, f"JIT {approx}{jitter}ms", m.f_sec)
    m.tb(d, m.FP_COL_R, m.FP_STAT2, f"DUTY {duty}%", m.f_sec)
    m.line(d, 0, m.FP_DIV, 127, m.FP_DIV)
    bits = m.pulse_train(period, burst)
    m.draw_trace(d, bits[shift:] + bits[:shift])
    return img


def verdict_frame(sv):
    verdict, advice, mx, av, field, hits = sv
    img, d = m.canvas()
    m.tb(d, 2, 9, "SITE SURVEY", m.f_sec)
    m.tb(d, 126, 9, "OK=again", m.f_sec, anchor="rs")
    m.line(d, 0, 11, 127, 11)
    alarm = verdict == "ACTIVE READER"
    if alarm:
        m.box(d, m.BANNER_X, m.BANNER_Y, m.BANNER_W, m.BANNER_H)
    else:
        m.frame(d, m.BANNER_X, m.BANNER_Y, m.BANNER_W, m.BANNER_H)
    m.tb(d, 64, m.DONE_V, verdict, m.f_pri, m.BG if alarm else m.FG, anchor="ms")
    m.tb(d, 64, m.DONE_A, advice, m.f_sec, anchor="ms")
    m.line(d, 0, m.DONE_A + 3, 127, m.DONE_A + 3)
    m.tb(d, 2, m.DONE_S1, f"MAX {mx}%", m.f_sec)
    m.tb(d, m.SV_COL_R, m.DONE_S1, f"AVG {av}%", m.f_sec)
    m.tb(d, 2, m.DONE_S2, f"HITS {hits}", m.f_sec)
    m.tb(d, m.SV_COL_R, m.DONE_S2, f"FIELD {field}%", m.f_sec)
    if alarm:
        m.frame(d, 0, 0, 127, 63, m.FG, lw=2)
    return img


def build():
    rows, fp, sv = engine_frames()
    frames, delays = [], []

    def add(img, ms=FRAME_MS):
        # Holds are durations, never repeated frames: Pillow's optimiser
        # collapses duplicates and the pause would silently disappear.
        frames.append(img)
        delays.append(ms)

    hist, anim = [], 0
    for i, r in enumerate(rows):
        anim += 1
        hist.append(r["shown"])
        # linger on the moment it pegs, and on the last frame before we cut away
        ms = FRAME_MS
        if r["sat"]:
            ms = 260
        if i == len(rows) - 1:
            ms = 700
        add(sweep_frame(r, hist, anim), ms=ms)

    conf_final = int(fp[2])
    for i in range(16):
        conf = min(conf_final, 18 + i * 7)
        add(fingerprint_frame(fp, conf, (i * 5) % 128),
            ms=(1100 if i == 15 else FRAME_MS))

    add(verdict_frame(sv), ms=2000)

    path = os.path.join(OUT, "demo.gif")
    # Full frames, not deltas. Pillow's optimiser writes only changed pixels and
    # assumes the previous frame stays underneath, so when a right-aligned string
    # gets shorter - STRONG -> MAX - the tail of the old word is left on screen.
    # The device redraws the whole canvas every tick; the GIF must too, or it
    # shows artefacts the app never produces. Two colours keeps it cheap anyway.
    frames[0].save(path, save_all=True, append_images=frames[1:], duration=delays,
                   loop=0, optimize=False, disposal=1, palette=1, colors=2)
    total = sum(delays) / 1000.0
    print(f"wrote {path}  ({len(frames)} frames, {total:.1f}s, "
          f"{os.path.getsize(path) // 1024} KB)")


if __name__ == "__main__":
    build()

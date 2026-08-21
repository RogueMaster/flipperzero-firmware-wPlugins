#!/usr/bin/env python3
"""Render the animated demo GIF for the README.

Reuses the drawing primitives from tools_gen_mockups so the animation is built
from the same layout constants as the real views - if a screen is wrong here it
is wrong on the device too.

The sequence tells the actual story of using the app: sweeping a quiet room,
closing in on something, locking onto it, fingerprinting what it is, then a
survey verdict. Roughly ten seconds, then it loops.

    python3 tools_gen_gif.py
"""
import os
import tools_gen_mockups as m

OUT = os.path.join(os.path.dirname(__file__), "images")
FRAME_MS = 100  # matches the app's own 100 ms UI tick


def sweep_frame(strength, peak, contacts, present, state, history, anim,
                sens="Custom", saturated=False, trend=None):
    img, d = m.canvas()
    m.draw_header(d, "SPECTER", state, present)
    m.draw_gauge(d, strength, peak, present, anim)
    m.draw_readout(d, strength, peak, contacts, trend)
    m.line(d, 0, 52, 127, 52)
    if present:
        m.box(d, 0, 53, 128, 11)
        m.disc(d, 4, 58, 1, m.BG)
        m.tb(d, 9, 61, "ACTIVE READER", m.f_sec, m.BG)
        m.tb(d, 125, 61, m.proximity_word(strength, saturated), m.f_sec, m.BG, anchor="rs")
        m.frame(d, 0, 0, 127, 63, m.FG, lw=2)
    else:
        label = f"S:{sens}"
        m.tb(d, 2, 62, label, m.f_sec)
        wave_left = 2 + int(d.textlength(label, font=m.f_sec) / m.S) + 4
        for k in range(62):
            x = 126 - k * 2
            if x < wave_left:
                break
            v = history[(len(history) - 1 - k) % len(history)]
            y = 63 - (v * 9) // 100
            if y < 63:
                m.line(d, x, 63, x, y, m.FG, w=m.S)
            else:
                m.dot(d, x, 63)
    return img


def fingerprint_frame(conf, period, burst, jitter, duty, shift):
    img, d = m.canvas()
    m.draw_header(d, "FINGERPRINT", "LISTENING", True)
    m.tb(d, 2, m.FP_CLASS, "POLLING", m.f_pri)
    m.frame(d, m.CONF_X, m.CONF_Y, m.CONF_W, m.CONF_H)
    fill = (conf * (m.CONF_W - 2)) // 100
    if fill:
        m.box(d, m.CONF_X + 1, m.CONF_Y + 1, fill, m.CONF_H - 2)
    m.tb(d, 2, m.FP_BLURB, "Fixed poll cycle", m.f_sec)
    m.tb(d, 126, m.FP_BLURB, f"{conf}%", m.f_sec, anchor="rs")
    m.tb(d, 2, m.FP_STAT1, f"PER {period}ms", m.f_sec)
    m.tb(d, m.FP_COL_R, m.FP_STAT1, f"BST {burst}ms", m.f_sec)
    m.tb(d, 2, m.FP_STAT2, f"JIT {jitter}ms", m.f_sec)
    m.tb(d, m.FP_COL_R, m.FP_STAT2, f"DUTY {duty}%", m.f_sec)
    m.line(d, 0, m.FP_DIV, 127, m.FP_DIV)
    bits = m.pulse_train(period, burst)
    m.draw_trace(d, bits[shift:] + bits[:shift])  # scroll the carrier trace
    return img


def verdict_frame():
    img, d = m.canvas()
    m.tb(d, 2, 9, "SITE SURVEY", m.f_sec)
    m.tb(d, 126, 9, "OK=again", m.f_sec, anchor="rs")
    m.line(d, 0, 11, 127, 11)
    m.box(d, m.BANNER_X, m.BANNER_Y, m.BANNER_W, m.BANNER_H)
    m.tb(d, 64, m.DONE_V, "ACTIVE READER", m.f_pri, m.BG, anchor="ms")
    m.tb(d, 64, m.DONE_A, "Fingerprint it", m.f_sec, anchor="ms")
    m.line(d, 0, m.DONE_A + 3, 127, m.DONE_A + 3)
    m.tb(d, 2, m.DONE_S1, "MAX 100%", m.f_sec)
    m.tb(d, m.SV_COL_R, m.DONE_S1, "AVG 34%", m.f_sec)
    m.tb(d, 2, m.DONE_S2, "HITS 3", m.f_sec)
    m.tb(d, m.SV_COL_R, m.DONE_S2, "FIELD 41%", m.f_sec)
    m.frame(d, 0, 0, 127, 63, m.FG, lw=2)
    return img


def build():
    frames, delays = [], []

    def add(img, ms=FRAME_MS):
        # Hold time is expressed as duration, never as repeated identical
        # frames - Pillow's optimiser collapses duplicates and the pause would
        # silently vanish.
        frames.append(img)
        delays.append(ms)

    hist = [3, 5, 2, 6, 4, 2, 7, 3, 5, 2, 8, 4, 3, 6, 2, 5, 3, 7, 4, 2] * 4
    anim = 0

    # 1. a quiet room - needle low, waveform flat
    for i in range(12):
        anim += 1
        add(sweep_frame(4 + (i % 3), 9, 0, False, "SCANNING", hist, anim, trend=0))

    # 2. closing in - the needle climbs and the trend arrow points up
    for s in (11, 19, 28, 38, 49, 58, 67, 74, 81, 88):
        anim += 1
        hist = hist[1:] + [s]
        add(sweep_frame(s, max(s, 9), 0, False, "SCANNING", hist, anim, trend=1), ms=170)

    # 3. locked on - alarm border, pegged meter
    for i in range(14):
        anim += 1
        add(sweep_frame(100, 100, 1, True, "READER", hist, anim, saturated=True),
            ms=(600 if i == 13 else FRAME_MS))

    # 4. what is it? the carrier's rhythm, scrolling
    for i in range(18):
        conf = min(88, 20 + i * 6)
        add(fingerprint_frame(conf, 204, 24, 2, 11, (i * 5) % 128),
            ms=(900 if i == 17 else FRAME_MS))

    # 5. the verdict
    add(verdict_frame(), ms=1900)

    path = os.path.join(OUT, "demo.gif")
    frames[0].save(
        path,
        save_all=True,
        append_images=frames[1:],
        duration=delays,
        loop=0,
        optimize=True,
        # the screen is two colours; say so and the file stays tiny
        palette=1,
        colors=2,
    )
    kb = os.path.getsize(path) // 1024
    print(f"wrote {path}  ({len(frames)} frames, {kb} KB)")


if __name__ == "__main__":
    build()
